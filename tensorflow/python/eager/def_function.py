# Copyright 2018 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
# pylint: disable=unidiomatic-typecheck
"""Prototype decorator for defining graph-mode functions with eager semantics."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import weakref

from tensorflow.python.eager import context
from tensorflow.python.eager import function as function_lib
from tensorflow.python.framework import ops
from tensorflow.python.ops import control_flow_ops
from tensorflow.python.ops import math_ops
from tensorflow.python.ops import resource_variable_ops
from tensorflow.python.ops import variable_scope
from tensorflow.python.training.checkpointable import base as checkpointable
from tensorflow.python.util import nest
from tensorflow.python.util import tf_decorator


class UnliftedInitializerVariable(resource_variable_ops.ResourceVariable):
  """Variable which does not lift its initializer out of function context.

  Instances of this variable, when created, build a graph which runs their
  initializer inside a tf.cond(is_initialized) block.

  This can only be created inside a defun called from (eventually) eager
  mode. That is, non-function-building graphs are not supported.
  """

  def __init__(self,  # pylint: disable=super-init-not-called
               initial_value=None,
               trainable=None,
               caching_device=None,
               name=None,
               dtype=None,
               constraint=None,
               **unused_kwargs):
    """Creates a variable.

    Args:
      initial_value: A `Tensor`, or Python object convertible to a `Tensor`,
        which is the initial value for the Variable. The initial value must have
        a shape specified unless `validate_shape` is set to False. Can also be a
        callable with no argument that returns the initial value when called.
        (Note that initializer functions from init_ops.py must first be bound
         to a shape before being used here.)
      trainable: If `True`, GradientTapes automatically watch uses of this
        Variable.
      caching_device: Optional device string or function describing where the
        Variable should be cached for reading.  Defaults to the Variable's
        device.  If not `None`, caches on another device.  Typical use is to
        cache on the device where the Ops using the Variable reside, to
        deduplicate copying through `Switch` and other conditional statements.
      name: Optional name for the variable. Defaults to `'Variable'` and gets
        uniquified automatically.
      dtype: If set, initial_value will be converted to the given type.
        If None, either the datatype will be kept (if initial_value is
       a Tensor) or float32 will be used (if it is a Python object convertible
       to a Tensor).
      constraint: An optional projection function to be applied to the variable
        after being updated by an `Optimizer` (e.g. used to implement norm
        constraints or value constraints for layer weights). The function must
        take as input the unprojected Tensor representing the value of the
        variable and return the Tensor for the projected value
        (which must have the same shape). Constraints are not safe to
        use when doing asynchronous distributed training.

    Raises:
      ValueError: If the initial value is not specified, or does not have a
        shape and `validate_shape` is `True`.
      RuntimeError: If called outside of a function definition.
    """
    if context.executing_eagerly():
      # If we've been init_scope()d out of the function definition nothing to do
      # here; we can't really do the capturing or conditional logic.
      resource_variable_ops.ResourceVariable.__init__(
          self, initial_value=initial_value, trainable=trainable,
          caching_device=caching_device, name=name, dtype=dtype,
          constraint=constraint)
      return
    with ops.init_scope():
      if not context.executing_eagerly():
        raise RuntimeError(
            "UnliftedInitializerVariable does not support legacy graph mode.")
    self._in_graph_mode = False
    if initial_value is None:
      raise ValueError("initial_value must be specified.")
    init_from_fn = callable(initial_value)

    if constraint is not None and not callable(constraint):
      raise ValueError("The `constraint` argument must be a callable.")

    if isinstance(initial_value, checkpointable.CheckpointInitialValue):
      self._maybe_initialize_checkpointable()
      self._update_uid = initial_value.checkpoint_position.restore_uid
      initial_value = initial_value.wrapped_value

    if trainable is None:
      trainable = True
    self._trainable = trainable
    self._save_slice_info = None
    self._initial_value = None
    self._initializer_op = None
    self._is_initialized_op = None
    self._graph_element = None
    self._cached_value = None
    # Store the graph key so optimizers know how to only retrieve variables from
    # this graph. Guaranteed to be the same as the eager graph_key.
    self._graph_key = ops.get_default_graph()._graph_key  # pylint: disable=protected-access
    with ops.name_scope(name, "Variable", []
                        if init_from_fn else [initial_value]) as name:
      # pylint: disable=protected-access
      with ops.init_scope():
        assert context.executing_eagerly()
        shared_name = ops._name_from_scope_name(name)
        shared_name = "%s_%d" % (shared_name, ops.uid())
      # Use attr_scope and device(None) to simulate the behavior of
      # colocate_with when the variable we want to colocate with doesn't
      # yet exist.
      with ops.name_scope("Initializer"), ops.device(None):
        initial_value = ops.convert_to_tensor(
            initial_value() if init_from_fn else initial_value,
            name="initial_value", dtype=dtype)
      with ops.init_scope():
        self._handle = resource_variable_ops.eager_safe_variable_handle(
            shape=initial_value.get_shape(),
            dtype=initial_value.dtype.base_dtype,
            shared_name=shared_name,
            name=name,
            graph_mode=False)
      self._shape = initial_value.shape
      self._unique_id = shared_name
      self._handle_name = shared_name + ":0"
      self._dtype = initial_value.dtype.base_dtype
      self._constraint = constraint
      assert initial_value is not None
      def assign_fn():
        with ops.name_scope("Assign") as n, ops.colocate_with(self._handle):
          resource_variable_ops.assign_variable_op(
              self._handle,
              initial_value,
              name=n)
        # Returning values to keep tf.cond happy.
        return ops.convert_to_tensor(1)
      def not_assign_fn():
        return ops.convert_to_tensor(0)
      # Note: this cond is always guaranteed to run because we're inside a defun
      # which will insert automatic control dependencies.
      control_flow_ops.cond(
          resource_variable_ops.var_is_initialized_op(self._handle),
          not_assign_fn, assign_fn)

    # After the handle has been created, set up a way to clean it up when
    # executing eagerly. We'll hold the only reference to the deleter, so that
    # when this object is garbage collected the deleter will be too. This
    # means ResourceVariables can be part of reference cycles without those
    # cycles being uncollectable.
    self._handle_deleter = resource_variable_ops.EagerResourceDeleter(
        handle=self._handle, handle_device=self._handle.device)
    self._cached_shape_as_list = None


def _defun_with_scope(scope, fn, input_signature):

  def wrapped_fn(*args, **kwds):
    with variable_scope.variable_creator_scope(scope):
      return fn(*args, **kwds)

  return function_lib.defun(tf_decorator.make_decorator(fn, wrapped_fn),
                            input_signature=input_signature)


# TODO(apassos) there should be an easier way to call a concrete defun.
def _call_concrete(fn, args, kwargs):
  """Calls the given concrete function with only the tensor arguments."""

  def inner():
    # TODO(apassos) figure out what to do with kwargs and concrete functions.
    return fn(*[t for t in nest.flatten((args, kwargs))
                if isinstance(
                    t, (ops.Tensor, resource_variable_ops.ResourceVariable))])

  return inner


class PolymorphicFunction(object):
  """Wrapper class for the graph functions defined for a Python function.

  See the documentation for `tf.function` for more information on the semantics
  of defined functions.

  PolymorphicFunction is thread-compatible.
  """

  def __init__(self,
               python_function,
               name,
               input_signature=None):
    """Initializes a polymorphic function.

    Args:
      python_function: the function to be wrapped.
      name: the name given to it.
      input_signature: a possibly nested sequence of `TensorSpec` objects
        specifying the input signature of this function. If `None`, a separate
        function is instantiated for each inferred input signature.

    Raises:
      ValueError: if `input_signature` is not None and the `python_function`'s
        argspec has keyword arguments.
    """
    self._python_function = python_function
    self._input_signature = input_signature
    self._created_variables = None
    self._stateful_fn = None
    self._descriptor_cache = weakref.WeakKeyDictionary()
    self._name = name

  def _initialize(self, args, kwds):
    """Initializes, on the first call."""

    self._created_variables = []

    def variable_capturing_scope(unused_next_creator, **kwds):
      """Creates UnliftedInitializerVariables and saves references to them."""
      v = UnliftedInitializerVariable(**kwds)
      self._created_variables.append(weakref.ref(v))
      return v

    self._stateful_fn = _defun_with_scope(
        variable_capturing_scope, self._python_function, self._input_signature)
    self._stateful_fn._name = self._name  # pylint: disable=protected-access

    # Force the definition of the function for these arguments
    self._concrete_stateful_fn = self._stateful_fn.get_concrete_function(
        *args, **kwds)

    def invalid_creator_scope(*unused_args, **unused_kwds):
      """Disables variable creation."""
      raise ValueError(
          "tf.function-decorated function tried to create "
          "variables on non-first call.")

    self._stateless_fn = _defun_with_scope(
        invalid_creator_scope, self._python_function, self._input_signature)
    self._stateless_fn._name = self._name  # pylint: disable=protected-access
    return self._stateful_fn._canonicalize_function_inputs(*args, **kwds)  # pylint: disable=protected-access

  def __call__(self, *args, **kwds):
    """Calls the graph function."""
    if self._created_variables:
      # In this case we have created variables on the first call, so we run the
      # defunned version which is guaranteed to never create variables.
      return self._stateless_fn(*args, **kwds)  # pylint: disable=not-callable
    elif self._stateful_fn is not None:
      # In this case we have not created variables on the first call. So we can
      # run the first trace but we should fail if variables are created.
      results = self._stateful_fn(*args, **kwds)
      if self._created_variables:
        raise ValueError("Creating variables on a non-first call to a function"
                         " decorated with tf.function.")
      return results

    canon_args, canon_kwds = self._initialize(args, kwds)

    if not self._created_variables:
      # If we did not create any variables the trace we have is good enough.
      return _call_concrete(
          self._concrete_stateful_fn, canon_args, canon_kwds)()

    def fn_with_cond(*inner_args, **inner_kwds):
      """Conditionally runs initialization if it's needed."""
      condition = True
      for wr in self._created_variables:
        variable = wr()
        if variable is None:
          raise ValueError(
              "Variable created in a tf.function garbage-collected. Code needs"
              " to keep python references to variables created in a"
              " tf.function.")
        condition = math_ops.logical_and(
            condition, resource_variable_ops.var_is_initialized_op(
                variable.handle))
      # We want to call stateless_fn if possible because it avoids recomputing
      # potentially expensive initializers.
      return control_flow_ops.cond(
          condition,
          lambda: self._stateless_fn(*inner_args, **inner_kwds),
          _call_concrete(self._concrete_stateful_fn, inner_args, inner_kwds))

    return function_lib.defun(fn_with_cond)(*canon_args, **canon_kwds)

  @property
  def python_function(self):
    """The python function wrapped in this tf.function."""
    return self._python_function

  def get_concrete_function(self, *args, **kwargs):
    """Returns a `Function` object specialized to inputs and execution context.

    `args` and `kwargs` are ignored if this `PolymorphicFunction` was created
    with an `input_signature`.

    Args:
      *args: inputs to specialize on.
      **kwargs: inputs to specialize on.

    Raises:
      ValueError: if this object has not yet been called on concrete values.
    """
    # TODO(apassos) figure out how to handle this case (what should we return
    # here?)
    if self._stateful_fn is None:
      raise ValueError(
          "Call this function with concrete values before asking for a"
          " concrete function. Calling the function will ensure that, in"
          " case this function creates variables, that those are properly"
          " initialized.")
    if self._created_variables:
      # In this case we have created variables on the first call, so we run the
      # defunned version which is guaranteed to never create variables.
      return self._stateless_fn.get_concrete_function(*args, **kwargs)
    elif self._stateful_fn is not None:
      # In this case we have not created variables on the first call. So we can
      # run the first trace but we should fail if variables are created.
      concrete = self._stateful_fn.get_concrete_function(*args, **kwargs)
      if self._created_variables:
        raise ValueError("Creating variables on a non-first call to a function"
                         " decorated with tf.function.")
      return concrete

  def __get__(self, instance, owner):
    """Makes it possible to defun instance methods."""
    del owner
    # `instance` here is the instance that this `PolymorphicFunction` was
    # accessed through; e.g., for
    #
    #   class Foo(object):
    #
    #     @function.defun
    #     def bar(self):
    #       ...
    #
    #   foo = Foo()
    #   foo.bar()  # `foo.bar` is a `PolymorphicFunction` instance
    #
    # then `instance` will be `foo` (and `owner` will be `Foo`).  We create a
    # new instance of PolymorphicFunction here to allow different instances each
    # to create variables once, thereby allowing methods to be decorated with
    # tf.function. Keeps a cache to avoid retracing the function every time the
    # descriptor is accessed.
    if instance not in self._descriptor_cache:
      if instance is None:
        return self
      self._descriptor_cache[instance] = (
          function_lib.class_method_to_instance_method(self, instance))
    return self._descriptor_cache[instance]


def function(func=None, input_signature=None):
  """Defines a function as per the "functions, not sessions" document."""
  if input_signature is not None:
    function_lib.validate_signature(input_signature)

  def decorated(inner_function):
    try:
      name = inner_function.__name__
    except AttributeError:
      name = "function"
    return tf_decorator.make_decorator(
        inner_function,
        PolymorphicFunction(
            inner_function,
            name,
            input_signature=input_signature))

  # This code path is for the `foo = tf.function(foo, ...)` use case
  if func is not None:
    return decorated(func)

  # This code path is for the
  #
  # @tf.function(...)
  # def foo(...):
  #    ...
  #
  # use case, which is equivalent to `foo = tf.function(...)(foo)`
  return decorated
