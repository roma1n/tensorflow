# Copyright 2015 The TensorFlow Authors. All Rights Reserved.
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

# pylint: disable=invalid-name
"""Save and restore variables."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import collections
import os.path
import time
import uuid

import numpy as np
import six

from tensorflow.core.protobuf import checkpointable_object_graph_pb2
from tensorflow.core.protobuf import meta_graph_pb2
from tensorflow.core.protobuf import saver_pb2
from tensorflow.python import pywrap_tensorflow
from tensorflow.python.client import session
from tensorflow.python.eager import context
from tensorflow.python.framework import constant_op
from tensorflow.python.framework import device as pydev
from tensorflow.python.framework import errors
from tensorflow.python.framework import meta_graph
from tensorflow.python.framework import ops
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import control_flow_ops
from tensorflow.python.ops import gen_io_ops
from tensorflow.python.ops import io_ops
from tensorflow.python.ops import resource_variable_ops
from tensorflow.python.ops import state_ops
from tensorflow.python.ops import string_ops
from tensorflow.python.ops import variables
from tensorflow.python.platform import gfile
from tensorflow.python.platform import tf_logging as logging
from tensorflow.python.training import checkpoint_management
from tensorflow.python.training import saveable_object
from tensorflow.python.training import training_util
from tensorflow.python.training.checkpointable import base as checkpointable
from tensorflow.python.util import compat
from tensorflow.python.util.tf_export import tf_export


# TODO(allenl): Remove these aliases once all users are migrated off.
get_checkpoint_state = checkpoint_management.get_checkpoint_state
update_checkpoint_state = checkpoint_management.update_checkpoint_state
generate_checkpoint_state_proto = (
    checkpoint_management.generate_checkpoint_state_proto)
latest_checkpoint = checkpoint_management.latest_checkpoint
checkpoint_exists = checkpoint_management.checkpoint_exists
get_checkpoint_mtimes = checkpoint_management.get_checkpoint_mtimes
remove_checkpoint = checkpoint_management.remove_checkpoint


# Op names which identify variable reads which should be saved.
_VARIABLE_OPS = set(["Variable",
                     "VariableV2",
                     "AutoReloadVariable",
                     "VarHandleOp",
                     "ReadVariableOp"])


def _set_cpu0(device_string):
  """Creates a new device string based on `device_string` but using /CPU:0.

  If the device is already on /CPU:0, this is a no-op.

  Args:
    device_string: A device string.

  Returns:
    A device string.
  """
  parsed_device = pydev.DeviceSpec.from_string(device_string)
  parsed_device.device_type = "CPU"
  parsed_device.device_index = 0
  return parsed_device.to_string()


class BaseSaverBuilder(object):
  """Base class for Savers.

  Can be extended to create different Ops.
  """

  SaveSpec = saveable_object.SaveSpec
  SaveableObject = saveable_object.SaveableObject

  class VariableSaveable(SaveableObject):
    """SaveableObject implementation that handles Variables."""

    def __init__(self, var, slice_spec, name):
      spec = BaseSaverBuilder.SaveSpec(var, slice_spec, name, dtype=var.dtype)
      super(BaseSaverBuilder.VariableSaveable, self).__init__(var, [spec], name)

    def restore(self, restored_tensors, restored_shapes):
      restored_tensor = restored_tensors[0]
      if restored_shapes is not None:
        restored_tensor = array_ops.reshape(restored_tensor, restored_shapes[0])
      return state_ops.assign(
          self.op,
          restored_tensor,
          validate_shape=restored_shapes is None and
          self.op.get_shape().is_fully_defined())

  class ResourceVariableSaveable(SaveableObject):
    """SaveableObject implementation that handles ResourceVariables."""

    def __init__(self, var, slice_spec, name):
      self._var_device = var.device
      self._var_shape = var.shape
      if isinstance(var, ops.Tensor):
        self.handle_op = var.op.inputs[0]
        tensor = var
      elif isinstance(var, resource_variable_ops.ResourceVariable):

        def _read_variable_closure(v):
          def f():
            with ops.device(v.device):
              x = v.read_value()
              # To allow variables placed on non-CPU devices to be checkpointed,
              # we copy them to CPU on the same machine first.
              with ops.device("/device:CPU:0"):
                return array_ops.identity(x)
          return f

        self.handle_op = var.handle
        tensor = _read_variable_closure(var)
      else:
        raise ValueError(
            "Saveable is neither a resource variable nor a read operation."
            " Got: %s" % repr(var))
      spec = BaseSaverBuilder.SaveSpec(tensor, slice_spec, name,
                                       dtype=var.dtype)
      super(BaseSaverBuilder.ResourceVariableSaveable, self).__init__(
          var, [spec], name)

    def restore(self, restored_tensors, restored_shapes):
      restored_tensor = restored_tensors[0]
      if restored_shapes is not None:
        restored_tensor = array_ops.reshape(restored_tensor, restored_shapes[0])
      # Copy the restored tensor to the variable's device.
      with ops.device(self._var_device):
        restored_tensor = array_ops.identity(restored_tensor)
        return resource_variable_ops.shape_safe_assign_variable_handle(
            self.handle_op, self._var_shape, restored_tensor)

  def __init__(self, write_version=saver_pb2.SaverDef.V2):
    self._write_version = write_version

  def save_op(self, filename_tensor, saveables):
    """Create an Op to save 'saveables'.

    This is intended to be overridden by subclasses that want to generate
    different Ops.

    Args:
      filename_tensor: String Tensor.
      saveables: A list of BaseSaverBuilder.SaveableObject objects.

    Returns:
      An Operation that save the variables.

    Raises:
      RuntimeError: (implementation detail) if "self._write_version" is an
        unexpected value.
    """
    # pylint: disable=protected-access
    tensor_names = []
    tensors = []
    tensor_slices = []
    for saveable in saveables:
      for spec in saveable.specs:
        tensor_names.append(spec.name)
        tensors.append(spec.tensor)
        tensor_slices.append(spec.slice_spec)
    if self._write_version == saver_pb2.SaverDef.V1:
      return io_ops._save(
          filename=filename_tensor,
          tensor_names=tensor_names,
          tensors=tensors,
          tensor_slices=tensor_slices)
    elif self._write_version == saver_pb2.SaverDef.V2:
      # "filename_tensor" is interpreted *NOT AS A FILENAME*, but as a prefix
      # of a V2 checkpoint: e.g. "/fs/train/ckpt-<step>/tmp/worker<i>-<step>".
      return io_ops.save_v2(filename_tensor, tensor_names, tensor_slices,
                            tensors)
    else:
      raise RuntimeError("Unexpected write_version: " + self._write_version)

  def bulk_restore(self, filename_tensor, saveables, preferred_shard,
                   restore_sequentially):
    """Restore all tensors contained in saveables.

    By default, this issues separate calls to `restore_op` for each saveable.
    Subclasses may override to load multiple saveables in a single call.

    Args:
      filename_tensor: String Tensor.
      saveables: List of BaseSaverBuilder.SaveableObject objects.
      preferred_shard: Int.  Shard to open first when loading a sharded file.
      restore_sequentially: Unused.  Bool.  If true, each restore is sequential.

    Returns:
      A list of Tensors resulting from reading 'saveable' from
        'filename'.

    """
    del restore_sequentially
    all_tensors = []
    for saveable in saveables:
      with ops.device(_set_cpu0(saveable.device) if saveable.device else None):
        all_tensors.extend(
            self.restore_op(filename_tensor, saveable, preferred_shard))
    return all_tensors

  # pylint: disable=unused-argument
  def restore_op(self, filename_tensor, saveable, preferred_shard):
    """Create ops to restore 'saveable'.

    This is intended to be overridden by subclasses that want to generate
    different Ops.

    Args:
      filename_tensor: String Tensor.
      saveable: A BaseSaverBuilder.SaveableObject object.
      preferred_shard: Int.  Shard to open first when loading a sharded file.

    Returns:
      A list of Tensors resulting from reading 'saveable' from
        'filename'.
    """
    # pylint: disable=protected-access
    tensors = []
    for spec in saveable.specs:
      tensors.append(
          io_ops.restore_v2(
              filename_tensor,
              [spec.name],
              [spec.slice_spec],
              [spec.dtype])[0])

    return tensors
  # pylint: enable=unused-argument

  def sharded_filename(self, filename_tensor, shard, num_shards):
    """Append sharding information to a filename.

    Args:
      filename_tensor: A string tensor.
      shard: Integer.  The shard for the filename.
      num_shards: An int Tensor for the number of shards.

    Returns:
      A string tensor.
    """
    return gen_io_ops.sharded_filename(filename_tensor, shard, num_shards)

  def _AddSaveOps(self, filename_tensor, saveables):
    """Add ops to save variables that are on the same shard.

    Args:
      filename_tensor: String Tensor.
      saveables: A list of SaveableObject objects.

    Returns:
      A tensor with the filename used to save.
    """
    save = self.save_op(filename_tensor, saveables)
    return control_flow_ops.with_dependencies([save], filename_tensor)

  def _AddShardedSaveOpsForV2(self, checkpoint_prefix, per_device):
    """Add ops to save the params per shard, for the V2 format.

    Note that the sharded save procedure for the V2 format is different from
    V1: there is a special "merge" step that merges the small metadata produced
    from each device.

    Args:
      checkpoint_prefix: scalar String Tensor.  Interpreted *NOT AS A
        FILENAME*, but as a prefix of a V2 checkpoint;
      per_device: A list of (device, BaseSaverBuilder.VarToSave) pairs, as
        returned by _GroupByDevices().

    Returns:
      An op to save the variables, which, when evaluated, returns the prefix
        "<user-fed prefix>" only and does not include the sharded spec suffix.
    """
    # IMPLEMENTATION DETAILS: most clients should skip.
    #
    # Suffix for any well-formed "checkpoint_prefix", when sharded.
    # Transformations:
    # * Users pass in "save_path" in save() and restore().  Say "myckpt".
    # * checkpoint_prefix gets fed <save_path><_SHARDED_SUFFIX>.
    #
    # Example:
    #   During runtime, a temporary directory is first created, which contains
    #   files
    #
    #     <train dir>/myckpt_temp/
    #        part-?????-of-?????{.index, .data-00000-of-00001}
    #
    #   Before .save() finishes, they will be (hopefully, atomically) renamed to
    #
    #     <train dir>/
    #        myckpt{.index, .data-?????-of-?????}
    #
    # Users only need to interact with the user-specified prefix, which is
    # "<train dir>/myckpt" in this case.  Save() and Restore() work with the
    # prefix directly, instead of any physical pathname.  (On failure and
    # subsequent restore, an outdated and orphaned temporary directory can be
    # safely removed.)
    _SHARDED_SUFFIX = "_temp_%s/part" % uuid.uuid4().hex
    tmp_checkpoint_prefix = string_ops.string_join(
        [checkpoint_prefix, _SHARDED_SUFFIX])

    num_shards = len(per_device)
    sharded_saves = []
    sharded_prefixes = []
    num_shards_tensor = constant_op.constant(num_shards, name="num_shards")
    last_device = None
    for shard, (device, saveables) in enumerate(per_device):
      last_device = device
      with ops.device(_set_cpu0(device)):
        sharded_filename = self.sharded_filename(tmp_checkpoint_prefix, shard,
                                                 num_shards_tensor)
        sharded_prefixes.append(sharded_filename)
        sharded_saves.append(self._AddSaveOps(sharded_filename, saveables))

    with ops.control_dependencies([x.op for x in sharded_saves]):
      # Co-locates the merge step with the last device.
      with ops.device(_set_cpu0(last_device)):
        # V2 format write path consists of a metadata merge step.  Once merged,
        # attempts to delete the temporary directory, "<user-fed prefix>_temp".
        merge_step = gen_io_ops.merge_v2_checkpoints(
            sharded_prefixes, checkpoint_prefix, delete_old_dirs=True)
        with ops.control_dependencies([merge_step]):
          # Returns the prefix "<user-fed prefix>" only.  DOES NOT include the
          # sharded spec suffix.
          return array_ops.identity(checkpoint_prefix)

  def _AddShardedSaveOps(self, filename_tensor, per_device):
    """Add ops to save the params per shard.

    Args:
      filename_tensor: a scalar String Tensor.
      per_device: A list of (device, BaseSaverBuilder.SaveableObject) pairs, as
        returned by _GroupByDevices().

    Returns:
      An op to save the variables.
    """
    if self._write_version == saver_pb2.SaverDef.V2:
      return self._AddShardedSaveOpsForV2(filename_tensor, per_device)

    num_shards = len(per_device)
    sharded_saves = []
    num_shards_tensor = constant_op.constant(num_shards, name="num_shards")
    for shard, (device, saveables) in enumerate(per_device):
      with ops.device(device):
        sharded_filename = self.sharded_filename(filename_tensor, shard,
                                                 num_shards_tensor)
        sharded_saves.append(self._AddSaveOps(sharded_filename, saveables))
    # Return the sharded name for the save path.
    with ops.control_dependencies([x.op for x in sharded_saves]):
      return gen_io_ops.sharded_filespec(filename_tensor, num_shards_tensor)

  def _AddRestoreOps(self,
                     filename_tensor,
                     saveables,
                     restore_sequentially,
                     reshape,
                     preferred_shard=-1,
                     name="restore_all"):
    """Add operations to restore saveables.

    Args:
      filename_tensor: Tensor for the path of the file to load.
      saveables: A list of SaveableObject objects.
      restore_sequentially: True if we want to restore variables sequentially
        within a shard.
      reshape: True if we want to reshape loaded tensors to the shape of
        the corresponding variable.
      preferred_shard: Shard to open first when loading a sharded file.
      name: Name for the returned op.

    Returns:
      An Operation that restores the variables.
    """
    all_tensors = self.bulk_restore(filename_tensor, saveables, preferred_shard,
                                    restore_sequentially)

    assign_ops = []
    idx = 0
    # Load and optionally reshape on the CPU, as string tensors are not
    # available on the GPU.
    # TODO(touts): Re-enable restore on GPU when we can support annotating
    # string tensors as "HostMemory" inputs.
    for saveable in saveables:
      shapes = None
      if reshape:
        # Compute the shapes, let the restore op decide if and how to do
        # the reshape.
        shapes = []
        for spec in saveable.specs:
          v = spec.tensor
          shape = v.get_shape()
          if not shape.is_fully_defined():
            shape = array_ops.shape(v)
          shapes.append(shape)
      saveable_tensors = all_tensors[idx:idx + len(saveable.specs)]
      idx += len(saveable.specs)
      assign_ops.append(saveable.restore(saveable_tensors, shapes))

    # Create a Noop that has control dependencies from all the updates.
    return control_flow_ops.group(*assign_ops, name=name)

  def _AddShardedRestoreOps(self, filename_tensor, per_device,
                            restore_sequentially, reshape):
    """Add Ops to restore variables from multiple devices.

    Args:
      filename_tensor: Tensor for the path of the file to load.
      per_device: A list of (device, SaveableObject) pairs, as
        returned by _GroupByDevices().
      restore_sequentially: True if we want to restore variables sequentially
        within a shard.
      reshape: True if we want to reshape loaded tensors to the shape of
        the corresponding variable.

    Returns:
      An Operation that restores the variables.
    """
    sharded_restores = []
    for shard, (device, saveables) in enumerate(per_device):
      with ops.device(device):
        sharded_restores.append(
            self._AddRestoreOps(
                filename_tensor,
                saveables,
                restore_sequentially,
                reshape,
                preferred_shard=shard,
                name="restore_shard"))
    return control_flow_ops.group(*sharded_restores, name="restore_all")

  @staticmethod
  def _IsVariable(v):
    return isinstance(v, ops.Tensor) and v.op.type in _VARIABLE_OPS

  def _GroupByDevices(self, saveables):
    """Group Variable tensor slices per device.

    TODO(touts): Make sure that all the devices found are on different
    job/replica/task/cpu|gpu.  It would be bad if 2 were on the same device.
    It can happen if the devices are unspecified.

    Args:
      saveables: A list of BaseSaverBuilder.SaveableObject objects.

    Returns:
      A list of tuples: (device_name, BaseSaverBuilder.SaveableObject) tuples.
      The list is sorted by ascending device_name.

    Raises:
      ValueError: If the tensors of a saveable are on different devices.
    """
    per_device = collections.defaultdict(lambda: [])
    for saveable in saveables:
      canonical_device = set(
          pydev.canonical_name(spec.tensor.device) for spec in saveable.specs)
      if len(canonical_device) != 1:
        raise ValueError("All tensors of a saveable object must be "
                         "on the same device: %s" % saveable.name)
      per_device[canonical_device.pop()].append(saveable)
    return sorted(per_device.items(), key=lambda t: t[0])

  @staticmethod
  def OpListToDict(op_list, convert_variable_to_tensor=True):
    """Create a dictionary of names to operation lists.

    Args:
      op_list: A list, tuple, or set of Variables or SaveableObjects.
      convert_variable_to_tensor: Whether or not to convert single Variables
        with no slice info into Tensors.

    Returns:
      A dictionary of names to the operations that must be saved under
      that name.  Variables with save_slice_info are grouped together under the
      same key in no particular order.

    Raises:
      TypeError: If the type of op_list or its elements is not supported.
      ValueError: If at least two saveables share the same name.
    """
    if not isinstance(op_list, (list, tuple, set)):
      raise TypeError("Variables to save should be passed in a dict or a "
                      "list: %s" % op_list)
    # When ResourceVariables are converted to Tensors, read ops are added to the
    # graph. Sorting the op_list ensures that the resulting graph is always
    # constructed in a deterministic way:
    op_list = sorted(op_list, key=lambda x: x.name)
    names_to_saveables = {}
    # pylint: disable=protected-access
    for var in op_list:
      if isinstance(var, BaseSaverBuilder.SaveableObject):
        names_to_saveables[var.name] = var
      elif isinstance(var, variables.PartitionedVariable):
        if var.name in names_to_saveables:
          raise ValueError("At least two variables have the same name: %s" %
                           var.name)
        names_to_saveables[var.name] = var
      elif isinstance(var, variables.Variable) and var._save_slice_info:
        name = var._save_slice_info.full_name
        if name in names_to_saveables:
          if not isinstance(names_to_saveables[name], list):
            raise ValueError("Mixing slices and non-slices with the same name: "
                             "%s" % name)
          names_to_saveables[name].append(var)
        else:
          names_to_saveables[name] = [var]
      elif (isinstance(var, checkpointable.CheckpointableBase)
            and not isinstance(var, variables.Variable)):
        checkpointable_saveables = [
            (factory() if callable(factory) else factory)
            for factory in var._gather_saveables_for_checkpoint().values()]
        names_to_saveables.update(
            BaseSaverBuilder.OpListToDict(checkpointable_saveables))
      else:
        if context.executing_eagerly():
          if not isinstance(var, resource_variable_ops.ResourceVariable):
            raise ValueError(
                "Can only save/restore ResourceVariables when eager execution "
                "is enabled, type: %s." % type(var))
          set_var = names_to_saveables.setdefault(var._shared_name, var)
          if set_var is not var:
            raise ValueError(
                ("Two different ResourceVariable objects with the same "
                 "shared_name '%s' were passed to the Saver. This likely means "
                 "that they were created in different Graphs or isolation "
                 "contexts, and may not be checkpointed together.") %
                (var._shared_name,))
        else:
          if convert_variable_to_tensor:
            if isinstance(var, resource_variable_ops.ResourceVariable):
              var = var._graph_element  # pylint: disable=protected-access
            else:
              var = ops.internal_convert_to_tensor(var, as_ref=True)
            if not BaseSaverBuilder._IsVariable(var):
              raise TypeError("Variable to save is not a Variable: %s" % var)
          if var.op.type == "ReadVariableOp":
            name = var.op.inputs[0].op.name
          else:
            name = var.op.name
          if name in names_to_saveables:
            raise ValueError("At least two variables have the same name: %s" %
                             name)
          names_to_saveables[name] = var

      # pylint: enable=protected-access
    return names_to_saveables

  @staticmethod
  def SaveableObjectsForOp(op, name):
    """Create `SaveableObject`s from an operation.

    Args:
      op: A variable, operation, or SaveableObject to coerce into a
        SaveableObject.
      name: A string name for the SaveableObject.

    Yields:
      `SaveableObject`s which together save/restore `op`.

    Raises:
      TypeError: If `name` is not a string.
      ValueError: For operations with no known conversion to SaveableObject.
    """
    if not isinstance(name, six.string_types):
      raise TypeError(
          "names_to_saveables must be a dict mapping string names to "
          "checkpointable operations. Name is not a string: %s" % name)
    if isinstance(op, BaseSaverBuilder.SaveableObject):
      yield op
    elif isinstance(op, (list, tuple, variables.PartitionedVariable)):
      if isinstance(op, variables.PartitionedVariable):
        op = list(op)
      # A set of slices.
      slice_name = None
      # pylint: disable=protected-access
      for variable in op:
        if not isinstance(variable, variables.Variable):
          raise ValueError("Slices must all be Variables: %s" % variable)
        if not variable._save_slice_info:
          raise ValueError("Slices must all be slices: %s" % variable)
        if slice_name is None:
          slice_name = variable._save_slice_info.full_name
        elif slice_name != variable._save_slice_info.full_name:
          raise ValueError(
              "Slices must all be from the same tensor: %s != %s" %
              (slice_name, variable._save_slice_info.full_name))
        if variable.op.type in ["Variable", "VariableV2",
                                "AutoReloadVariable"]:
          yield BaseSaverBuilder.VariableSaveable(
              variable, variable._save_slice_info.spec, name)
        else:
          yield BaseSaverBuilder.ResourceVariableSaveable(
              variable, variable._save_slice_info.spec, name)
      # pylint: enable=protected-access
    elif isinstance(op, checkpointable.CheckpointableBase) and not isinstance(
        op, variables.Variable):
      # pylint: disable=protected-access
      for attr, factory in op._gather_saveables_for_checkpoint().items():
        op = (factory(name + "_" + attr) if callable(factory) else factory)
        for op in BaseSaverBuilder.SaveableObjectsForOp(op, op.name):
          yield op
      # pylint: enable=protected-access
    else:
      # A variable or tensor.
      if context.executing_eagerly():
        if not isinstance(op, resource_variable_ops.ResourceVariable):
          raise ValueError("Can only save/restore ResourceVariable eager "
                           "mode is enabled, type: %s." % type(op))
        yield BaseSaverBuilder.ResourceVariableSaveable(op, "", name)
      else:
        if isinstance(op, resource_variable_ops.ResourceVariable):
          variable = op._graph_element  # pylint: disable=protected-access
        else:
          variable = ops.internal_convert_to_tensor(op, as_ref=True)
        if not BaseSaverBuilder._IsVariable(variable):
          raise TypeError("names_to_saveables must be a dict mapping string "
                          "names to Tensors/Variables. Not a variable: %s" %
                          variable)
        if variable.op.type in ["Variable", "VariableV2",
                                "AutoReloadVariable"]:
          yield BaseSaverBuilder.VariableSaveable(variable, "", name)
        else:
          yield BaseSaverBuilder.ResourceVariableSaveable(
              variable, "", name)

  def _ValidateAndSliceInputs(self, names_to_saveables):
    """Returns the variables and names that will be used for a Saver.

    Args:
      names_to_saveables: A dict (k, v) where k is the name of an operation and
         v is an operation to save or a BaseSaverBuilder.Saver.

    Returns:
      A list of BaseSaverBuilder.SaveableObject objects.

    Raises:
      TypeError: If any of the keys are not strings or any of the
        values are not one of Tensor or Variable or a checkpointable operation.
      ValueError: If the same operation is given in more than one value
        (this also applies to slices of SlicedVariables).
    """
    if not isinstance(names_to_saveables, dict):
      names_to_saveables = BaseSaverBuilder.OpListToDict(names_to_saveables)

    saveables = []
    seen_ops = set()
    for name, op in sorted(names_to_saveables.items(),
                           # Avoid comparing ops, sort only by name.
                           key=lambda x: x[0]):
      for converted_saveable_object in self.SaveableObjectsForOp(op, name):
        self._AddSaveable(saveables, seen_ops, converted_saveable_object)
    return saveables

  def _AddSaveable(self, saveables, seen_ops, saveable):
    """Adds the saveable to the saveables list.

    Args:
      saveables: List to append the SaveableObject to.
      seen_ops: Set of the ops of the saveables already processed.  Used to
        check that each saveable is only saved once.
      saveable: The saveable.

    Raises:
      ValueError: If the saveable has already been processed.
    """
    if saveable.op in seen_ops:
      raise ValueError("The same saveable will be restored with two names: %s" %
                       saveable.name)
    saveables.append(saveable)
    seen_ops.add(saveable.op)

  def build(self,
            names_to_saveables,
            reshape=False,
            sharded=False,
            max_to_keep=5,
            keep_checkpoint_every_n_hours=10000.0,
            name=None,
            restore_sequentially=False,
            filename="model"):
    """Builds save/restore graph nodes or runs save/restore in eager mode.

    Args:
      names_to_saveables: A dictionary mapping name to a Variable or
        SaveableObject. Each name will be associated with the
        corresponding variable in the checkpoint.
      reshape: If True, allow restoring parameters from a checkpoint
        that where the parameters have a different shape.  This is
        only needed when you try to restore from a Dist-Belief checkpoint,
        and only some times.
      sharded: If True, shard the checkpoints, one per device that has
        Variable nodes.
      max_to_keep: Maximum number of checkpoints to keep.  As new checkpoints
        are created, old ones are deleted.  If None or 0, no checkpoints are
        deleted from the filesystem but only the last one is kept in the
        `checkpoint` file.  Presently the number is only roughly enforced.  For
        example in case of restarts more than max_to_keep checkpoints may be
        kept.
      keep_checkpoint_every_n_hours: How often checkpoints should be kept.
        Defaults to 10,000 hours.
      name: String.  Optional name to use as a prefix when adding operations.
      restore_sequentially: A Bool, which if true, causes restore of different
        variables to happen sequentially within each device.
      filename: If known at graph construction time, filename used for variable
        loading/saving. If None, then the default name "model" will be used.

    Returns:
      A SaverDef proto.

    Raises:
      TypeError: If 'names_to_saveables' is not a dictionary mapping string
        keys to variable Tensors.
      ValueError: If any of the keys or values in 'names_to_saveables' is not
        unique.
    """
    return self._build_internal(
        names_to_saveables=names_to_saveables,
        reshape=reshape,
        sharded=sharded,
        max_to_keep=max_to_keep,
        keep_checkpoint_every_n_hours=keep_checkpoint_every_n_hours,
        name=name,
        restore_sequentially=restore_sequentially,
        filename=filename)

  def _build_internal(self,
                      names_to_saveables,
                      reshape=False,
                      sharded=False,
                      max_to_keep=5,
                      keep_checkpoint_every_n_hours=10000.0,
                      name=None,
                      restore_sequentially=False,
                      filename="model",
                      build_save=True,
                      build_restore=True):
    """build() with option to only perform save and restore."""
    if not context.executing_eagerly() and (not build_save or
                                            not build_restore):
      raise ValueError("save and restore operations need to be built together "
                       " when eager execution is not enabled.")

    saveables = self._ValidateAndSliceInputs(names_to_saveables)
    if max_to_keep is None:
      max_to_keep = 0

    with ops.name_scope(name, "save",
                        [saveable.op for saveable in saveables]) as name:
      # Add the Constant string tensor for the filename.
      filename_tensor = constant_op.constant(filename or "model")

      # Add the save ops.
      if sharded:
        per_device = self._GroupByDevices(saveables)
        if build_save:
          save_tensor = self._AddShardedSaveOps(filename_tensor, per_device)
        if build_restore:
          restore_op = self._AddShardedRestoreOps(filename_tensor, per_device,
                                                  restore_sequentially, reshape)
      else:
        if build_save:
          save_tensor = self._AddSaveOps(filename_tensor, saveables)
        if build_restore:
          restore_op = self._AddRestoreOps(filename_tensor, saveables,
                                           restore_sequentially, reshape)

    # In the following use case, it's possible to have restore_ops be called
    # something else:
    # - Build inference graph and export a meta_graph.
    # - Import the inference meta_graph
    # - Extend the inference graph to a train graph.
    # - Export a new meta_graph.
    # Now the second restore_op will be called "restore_all_1".
    # As such, comment out the assert for now until we know whether supporting
    # such usage model makes sense.
    #
    # assert restore_op.name.endswith("restore_all"), restore_op.name
    if context.executing_eagerly():
      # Store the tensor values to the tensor_names.
      save_tensor_name = save_tensor.numpy() if build_save else ""
      return saver_pb2.SaverDef(
          filename_tensor_name=filename_tensor.numpy(),
          save_tensor_name=save_tensor_name,
          restore_op_name="",
          max_to_keep=max_to_keep,
          sharded=sharded,
          keep_checkpoint_every_n_hours=keep_checkpoint_every_n_hours,
          version=self._write_version)
    else:
      graph = ops.get_default_graph()
      # Do some sanity checking on collections containing
      # PartitionedVariables. If a saved collection has a PartitionedVariable,
      # the GraphDef needs to include concat ops to get the value (or there'll
      # be a lookup error on load).
      check_collection_list = graph.get_all_collection_keys()
      for collection_type in check_collection_list:
        for element in graph.get_collection(collection_type):
          if isinstance(element, variables.PartitionedVariable):
            try:
              graph.get_operation_by_name(element.name)
            except KeyError:
              # Create a concat op for this PartitionedVariable. The user may
              # not need it, but we'll try looking it up on MetaGraph restore
              # since it's in a collection.
              element.as_tensor()
      return saver_pb2.SaverDef(
          filename_tensor_name=filename_tensor.name,
          save_tensor_name=save_tensor.name,
          restore_op_name=restore_op.name,
          max_to_keep=max_to_keep,
          sharded=sharded,
          keep_checkpoint_every_n_hours=keep_checkpoint_every_n_hours,
          version=self._write_version)


class BulkSaverBuilder(BaseSaverBuilder):
  """SaverBuilder with support for bulk restoring multiple saveables."""

  def bulk_restore(self, filename_tensor, saveables, preferred_shard,
                   restore_sequentially):

    # Ignored: bulk restore is internally sequential.
    del restore_sequentially
    restore_specs = []
    for saveable in saveables:
      for spec in saveable.specs:
        restore_specs.append((spec.name, spec.slice_spec, spec.dtype))

    names, slices, dtypes = zip(*restore_specs)
    # Load all tensors onto CPU 0 for compatibility with existing code.
    with ops.device("cpu:0"):
      return io_ops.restore_v2(filename_tensor, names, slices, dtypes)


def _get_saver_or_default():
  """Returns the saver from SAVERS collection, or creates a default one.

  This method is used by other members of the training module, such as
  `Scaffold`, or `CheckpointSaverHook`.

  Returns:
    `Saver`.

  Raises:
    RuntimeError: If the SAVERS collection already has more than one items.
  """
  collection_key = ops.GraphKeys.SAVERS
  savers = ops.get_collection(collection_key)
  if savers:
    if len(savers) > 1:
      raise RuntimeError(
          "More than one item in collection {}. "
          "Please indicate which one to use by passing it to the constructor.".
          format(collection_key))
    return savers[0]
  saver = Saver(sharded=True, allow_empty=True)
  if saver is not None:
    ops.add_to_collection(collection_key, saver)
  return saver


@tf_export("train.Saver")
class Saver(object):
  """Saves and restores variables.

  See [Variables](https://tensorflow.org/guide/variables)
  for an overview of variables, saving and restoring.

  The `Saver` class adds ops to save and restore variables to and from
  *checkpoints*.  It also provides convenience methods to run these ops.

  Checkpoints are binary files in a proprietary format which map variable names
  to tensor values.  The best way to examine the contents of a checkpoint is to
  load it using a `Saver`.

  Savers can automatically number checkpoint filenames with a provided counter.
  This lets you keep multiple checkpoints at different steps while training a
  model.  For example you can number the checkpoint filenames with the training
  step number.  To avoid filling up disks, savers manage checkpoint files
  automatically. For example, they can keep only the N most recent files, or
  one checkpoint for every N hours of training.

  You number checkpoint filenames by passing a value to the optional
  `global_step` argument to `save()`:

  ```python
  saver.save(sess, 'my-model', global_step=0) ==> filename: 'my-model-0'
  ...
  saver.save(sess, 'my-model', global_step=1000) ==> filename: 'my-model-1000'
  ```

  Additionally, optional arguments to the `Saver()` constructor let you control
  the proliferation of checkpoint files on disk:

  * `max_to_keep` indicates the maximum number of recent checkpoint files to
    keep.  As new files are created, older files are deleted.   If None or 0,
    no checkpoints are deleted from the filesystem but only the last one is
    kept in the `checkpoint` file.  Defaults to 5 (that is, the 5 most recent
    checkpoint files are kept.)

  * `keep_checkpoint_every_n_hours`: In addition to keeping the most recent
    `max_to_keep` checkpoint files, you might want to keep one checkpoint file
    for every N hours of training.  This can be useful if you want to later
    analyze how a model progressed during a long training session.  For
    example, passing `keep_checkpoint_every_n_hours=2` ensures that you keep
    one checkpoint file for every 2 hours of training.  The default value of
    10,000 hours effectively disables the feature.

  Note that you still have to call the `save()` method to save the model.
  Passing these arguments to the constructor will not save variables
  automatically for you.

  A training program that saves regularly looks like:

  ```python
  ...
  # Create a saver.
  saver = tf.train.Saver(...variables...)
  # Launch the graph and train, saving the model every 1,000 steps.
  sess = tf.Session()
  for step in xrange(1000000):
      sess.run(..training_op..)
      if step % 1000 == 0:
          # Append the step number to the checkpoint name:
          saver.save(sess, 'my-model', global_step=step)
  ```

  In addition to checkpoint files, savers keep a protocol buffer on disk with
  the list of recent checkpoints. This is used to manage numbered checkpoint
  files and by `latest_checkpoint()`, which makes it easy to discover the path
  to the most recent checkpoint. That protocol buffer is stored in a file named
  'checkpoint' next to the checkpoint files.

  If you create several savers, you can specify a different filename for the
  protocol buffer file in the call to `save()`.
  """

  def __init__(self,
               var_list=None,
               reshape=False,
               sharded=False,
               max_to_keep=5,
               keep_checkpoint_every_n_hours=10000.0,
               name=None,
               restore_sequentially=False,
               saver_def=None,
               builder=None,
               defer_build=False,
               allow_empty=False,
               write_version=saver_pb2.SaverDef.V2,
               pad_step_number=False,
               save_relative_paths=False,
               filename=None):
    """Creates a `Saver`.

    The constructor adds ops to save and restore variables.

    `var_list` specifies the variables that will be saved and restored. It can
    be passed as a `dict` or a list:

    * A `dict` of names to variables: The keys are the names that will be
      used to save or restore the variables in the checkpoint files.
    * A list of variables: The variables will be keyed with their op name in
      the checkpoint files.

    For example:

    ```python
    v1 = tf.Variable(..., name='v1')
    v2 = tf.Variable(..., name='v2')

    # Pass the variables as a dict:
    saver = tf.train.Saver({'v1': v1, 'v2': v2})

    # Or pass them as a list.
    saver = tf.train.Saver([v1, v2])
    # Passing a list is equivalent to passing a dict with the variable op names
    # as keys:
    saver = tf.train.Saver({v.op.name: v for v in [v1, v2]})
    ```

    The optional `reshape` argument, if `True`, allows restoring a variable from
    a save file where the variable had a different shape, but the same number
    of elements and type.  This is useful if you have reshaped a variable and
    want to reload it from an older checkpoint.

    The optional `sharded` argument, if `True`, instructs the saver to shard
    checkpoints per device.

    Args:
      var_list: A list of `Variable`/`SaveableObject`, or a dictionary mapping
        names to `SaveableObject`s. If `None`, defaults to the list of all
        saveable objects.
      reshape: If `True`, allows restoring parameters from a checkpoint
        where the variables have a different shape.
      sharded: If `True`, shard the checkpoints, one per device.
      max_to_keep: Maximum number of recent checkpoints to keep.
        Defaults to 5.
      keep_checkpoint_every_n_hours: How often to keep checkpoints.
        Defaults to 10,000 hours.
      name: String.  Optional name to use as a prefix when adding operations.
      restore_sequentially: A `Bool`, which if true, causes restore of different
        variables to happen sequentially within each device.  This can lower
        memory usage when restoring very large models.
      saver_def: Optional `SaverDef` proto to use instead of running the
        builder. This is only useful for specialty code that wants to recreate
        a `Saver` object for a previously built `Graph` that had a `Saver`.
        The `saver_def` proto should be the one returned by the
        `as_saver_def()` call of the `Saver` that was created for that `Graph`.
      builder: Optional `SaverBuilder` to use if a `saver_def` was not provided.
        Defaults to `BulkSaverBuilder()`.
      defer_build: If `True`, defer adding the save and restore ops to the
        `build()` call. In that case `build()` should be called before
        finalizing the graph or using the saver.
      allow_empty: If `False` (default) raise an error if there are no
        variables in the graph. Otherwise, construct the saver anyway and make
        it a no-op.
      write_version: controls what format to use when saving checkpoints.  It
        also affects certain filepath matching logic.  The V2 format is the
        recommended choice: it is much more optimized than V1 in terms of
        memory required and latency incurred during restore.  Regardless of
        this flag, the Saver is able to restore from both V2 and V1 checkpoints.
      pad_step_number: if True, pads the global step number in the checkpoint
        filepaths to some fixed width (8 by default).  This is turned off by
        default.
      save_relative_paths: If `True`, will write relative paths to the
        checkpoint state file. This is needed if the user wants to copy the
        checkpoint directory and reload from the copied directory.
      filename: If known at graph construction time, filename used for variable
        loading/saving.

    Raises:
      TypeError: If `var_list` is invalid.
      ValueError: If any of the keys or values in `var_list` are not unique.
      RuntimeError: If eager execution is enabled and`var_list` does not specify
        a list of varialbes to save.

    @compatibility(eager)
    When eager execution is enabled, `var_list` must specify a `list` or `dict`
    of variables to save. Otherwise, a `RuntimeError` will be raised.
    @end_compatibility
    """
    if defer_build and var_list:
      raise ValueError(
          "If `var_list` is provided then build cannot be deferred. "
          "Either set defer_build=False or var_list=None.")
    if context.executing_eagerly() and var_list is None:
      raise RuntimeError(
          "When eager execution is enabled, `var_list` must specify a list or "
          "dict of variables to save")
    self._var_list = var_list
    self._reshape = reshape
    self._sharded = sharded
    self._max_to_keep = max_to_keep
    self._keep_checkpoint_every_n_hours = keep_checkpoint_every_n_hours
    self._name = name
    self._restore_sequentially = restore_sequentially
    self.saver_def = saver_def
    self._builder = builder
    self._is_built = False
    self._allow_empty = allow_empty
    self._is_empty = None
    self._write_version = write_version
    self._pad_step_number = pad_step_number
    self._filename = filename
    self._last_checkpoints = []
    self._checkpoints_to_be_deleted = []
    if context.executing_eagerly():
      self._next_checkpoint_time = (
          time.time() + self._keep_checkpoint_every_n_hours * 3600)
    elif not defer_build:
      self.build()
    if self.saver_def:
      self._check_saver_def()
      self._write_version = self.saver_def.version
    self._save_relative_paths = save_relative_paths
    # For compatibility with object-based checkpoints, we may build a second
    # Saver to read the renamed keys.
    self._object_restore_saver = None

  def build(self):
    if context.executing_eagerly():
      raise RuntimeError("Use save/restore instead of build in eager mode.")
    self._build(self._filename, build_save=True, build_restore=True)

  def _build_eager(self, checkpoint_path, build_save, build_restore):
    self._build(
        checkpoint_path, build_save=build_save, build_restore=build_restore)

  def _build(self, checkpoint_path, build_save, build_restore):
    """Builds saver_def."""
    if not context.executing_eagerly():
      if self._is_built:
        return
      self._is_built = True

    if not self.saver_def or context.executing_eagerly():
      if self._builder is None:
        self._builder = BulkSaverBuilder(self._write_version)

      if self._var_list is None:
        # pylint: disable=protected-access
        self._var_list = variables._all_saveable_objects()
      if not self._var_list:
        if self._allow_empty:
          self._is_empty = True
          return
        else:
          raise ValueError("No variables to save")
      self._is_empty = False

      self.saver_def = self._builder._build_internal(  # pylint: disable=protected-access
          self._var_list,
          reshape=self._reshape,
          sharded=self._sharded,
          max_to_keep=self._max_to_keep,
          keep_checkpoint_every_n_hours=self._keep_checkpoint_every_n_hours,
          name=self._name,
          restore_sequentially=self._restore_sequentially,
          filename=checkpoint_path,
          build_save=build_save, build_restore=build_restore)
    elif self.saver_def and self._name:
      # Since self._name is used as a name_scope by builder(), we are
      # overloading the use of this field to represent the "import_scope" as
      # well.
      self.saver_def.filename_tensor_name = ops.prepend_name_scope(
          self.saver_def.filename_tensor_name, self._name)
      self.saver_def.save_tensor_name = ops.prepend_name_scope(
          self.saver_def.save_tensor_name, self._name)
      self.saver_def.restore_op_name = ops.prepend_name_scope(
          self.saver_def.restore_op_name, self._name)

    self._check_saver_def()
    if not context.executing_eagerly():
      # Updates next checkpoint time.
      # Set in __init__ when executing eagerly.
      self._next_checkpoint_time = (
          time.time() + self.saver_def.keep_checkpoint_every_n_hours * 3600)

  def _check_saver_def(self):
    if not isinstance(self.saver_def, saver_pb2.SaverDef):
      raise ValueError("saver_def must be a saver_pb2.SaverDef: %s" %
                       self.saver_def)
    if not context.executing_eagerly():
      if not self.saver_def.save_tensor_name:
        raise ValueError("saver_def must specify the save_tensor_name: %s" %
                         str(self.saver_def))
      if not self.saver_def.restore_op_name:
        raise ValueError("saver_def must specify the restore_op_name: %s" %
                         str(self.saver_def))

  def _CheckpointFilename(self, p):
    """Returns the checkpoint filename given a `(filename, time)` pair.

    Args:
      p: (filename, time) pair.

    Returns:
      Checkpoint file name.
    """
    name, _ = p
    return name

  def _RecordLastCheckpoint(self, latest_save_path):
    """Manages the list of the latest checkpoints."""
    if not self.saver_def.max_to_keep:
      return
    # Remove first from list if the same name was used before.
    for p in self._last_checkpoints:
      if latest_save_path == self._CheckpointFilename(p):
        self._last_checkpoints.remove(p)
    # Append new path to list
    self._last_checkpoints.append((latest_save_path, time.time()))

    # If more than max_to_keep, remove oldest.
    if len(self._last_checkpoints) > self.saver_def.max_to_keep:
      self._checkpoints_to_be_deleted.append(self._last_checkpoints.pop(0))

  def _MaybeDeleteOldCheckpoints(self, meta_graph_suffix="meta"):
    """Deletes old checkpoints if necessary.

    `self._checkpoints_to_be_deleted` is going to contain checkpoints that are
    over `max_to_keep`.  They are going to be deleted.  If
    `keep_checkpoint_every_n_hours` was specified, keep an additional checkpoint
    every `N` hours. For example, if `N` is 0.5, an additional checkpoint is
    kept for every 0.5 hours of training; if `N` is 10, an additional
    checkpoint is kept for every 10 hours of training.

    Args:
      meta_graph_suffix: Suffix for `MetaGraphDef` file. Defaults to 'meta'.
    """
    if self._checkpoints_to_be_deleted:
      p = self._checkpoints_to_be_deleted.pop(0)
      # Do not delete the file if we keep_checkpoint_every_n_hours is set and we
      # have reached N hours of training.
      should_keep = p[1] > self._next_checkpoint_time
      if should_keep:
        self._next_checkpoint_time += (
            self.saver_def.keep_checkpoint_every_n_hours * 3600)
        return

      # Otherwise delete the files.
      try:
        checkpoint_management.remove_checkpoint(
            self._CheckpointFilename(p), self.saver_def.version,
            meta_graph_suffix)
      except Exception as e:  # pylint: disable=broad-except
        logging.warning("Ignoring: %s", str(e))

  def as_saver_def(self):
    """Generates a `SaverDef` representation of this saver.

    Returns:
      A `SaverDef` proto.
    """
    return self.saver_def

  def to_proto(self, export_scope=None):
    """Converts this `Saver` to a `SaverDef` protocol buffer.

    Args:
      export_scope: Optional `string`. Name scope to remove.

    Returns:
      A `SaverDef` protocol buffer.
    """
    if export_scope is None:
      return self.saver_def

    if not (self.saver_def.filename_tensor_name.startswith(export_scope) and
            self.saver_def.save_tensor_name.startswith(export_scope) and
            self.saver_def.restore_op_name.startswith(export_scope)):
      return None

    saver_def = saver_pb2.SaverDef()
    saver_def.CopyFrom(self.saver_def)
    saver_def.filename_tensor_name = ops.strip_name_scope(
        saver_def.filename_tensor_name, export_scope)
    saver_def.save_tensor_name = ops.strip_name_scope(
        saver_def.save_tensor_name, export_scope)
    saver_def.restore_op_name = ops.strip_name_scope(
        saver_def.restore_op_name, export_scope)
    return saver_def

  @staticmethod
  def from_proto(saver_def, import_scope=None):
    """Returns a `Saver` object created from `saver_def`.

    Args:
      saver_def: a `SaverDef` protocol buffer.
      import_scope: Optional `string`. Name scope to use.

    Returns:
      A `Saver` built from saver_def.
    """
    return Saver(saver_def=saver_def, name=import_scope)

  @property
  def last_checkpoints(self):
    """List of not-yet-deleted checkpoint filenames.

    You can pass any of the returned values to `restore()`.

    Returns:
      A list of checkpoint filenames, sorted from oldest to newest.
    """
    return list(self._CheckpointFilename(p) for p in self._last_checkpoints)

  def set_last_checkpoints(self, last_checkpoints):
    """DEPRECATED: Use set_last_checkpoints_with_time.

    Sets the list of old checkpoint filenames.

    Args:
      last_checkpoints: A list of checkpoint filenames.

    Raises:
      AssertionError: If last_checkpoints is not a list.
    """
    assert isinstance(last_checkpoints, list)
    # We use a timestamp of +inf so that this checkpoint will never be
    # deleted.  This is both safe and backwards compatible to a previous
    # version of the code which used s[1] as the "timestamp".
    self._last_checkpoints = [(s, np.inf) for s in last_checkpoints]

  def set_last_checkpoints_with_time(self, last_checkpoints_with_time):
    """Sets the list of old checkpoint filenames and timestamps.

    Args:
      last_checkpoints_with_time: A list of tuples of checkpoint filenames and
        timestamps.

    Raises:
      AssertionError: If last_checkpoints_with_time is not a list.
    """
    assert isinstance(last_checkpoints_with_time, list)
    self._last_checkpoints = last_checkpoints_with_time

  def recover_last_checkpoints(self, checkpoint_paths):
    """Recovers the internal saver state after a crash.

    This method is useful for recovering the "self._last_checkpoints" state.

    Globs for the checkpoints pointed to by `checkpoint_paths`.  If the files
    exist, use their mtime as the checkpoint timestamp.

    Args:
      checkpoint_paths: a list of checkpoint paths.
    """
    mtimes = checkpoint_management.get_checkpoint_mtimes(checkpoint_paths)
    self.set_last_checkpoints_with_time(list(zip(checkpoint_paths, mtimes)))

  def save(self,
           sess,
           save_path,
           global_step=None,
           latest_filename=None,
           meta_graph_suffix="meta",
           write_meta_graph=True,
           write_state=True,
           strip_default_attrs=False):
    # pylint: disable=line-too-long
    """Saves variables.

    This method runs the ops added by the constructor for saving variables.
    It requires a session in which the graph was launched.  The variables to
    save must also have been initialized.

    The method returns the path prefix of the newly created checkpoint files.
    This string can be passed directly to a call to `restore()`.

    Args:
      sess: A Session to use to save the variables.
      save_path: String.  Prefix of filenames created for the checkpoint.
      global_step: If provided the global step number is appended to
        `save_path` to create the checkpoint filenames. The optional argument
        can be a `Tensor`, a `Tensor` name or an integer.
      latest_filename: Optional name for the protocol buffer file that will
        contains the list of most recent checkpoints.  That file,
        kept in the same directory as the checkpoint files, is automatically
        managed by the saver to keep track of recent checkpoints.  Defaults to
        'checkpoint'.
      meta_graph_suffix: Suffix for `MetaGraphDef` file. Defaults to 'meta'.
      write_meta_graph: `Boolean` indicating whether or not to write the meta
        graph file.
      write_state: `Boolean` indicating whether or not to write the
        `CheckpointStateProto`.
      strip_default_attrs: Boolean. If `True`, default-valued attributes will be
        removed from the NodeDefs. For a detailed guide, see
        [Stripping Default-Valued Attributes](https://github.com/tensorflow/tensorflow/blob/master/tensorflow/python/saved_model/README.md#stripping-default-valued-attributes).

    Returns:
      A string: path prefix used for the checkpoint files.  If the saver is
        sharded, this string ends with: '-?????-of-nnnnn' where 'nnnnn'
        is the number of shards created.
      If the saver is empty, returns None.

    Raises:
      TypeError: If `sess` is not a `Session`.
      ValueError: If `latest_filename` contains path components, or if it
        collides with `save_path`.
      RuntimeError: If save and restore ops weren't built.
    """
    # pylint: enable=line-too-long
    if not self._is_built and not context.executing_eagerly():
      raise RuntimeError(
          "`build()` should be called before save if defer_build==True")
    if latest_filename is None:
      latest_filename = "checkpoint"
    if self._write_version != saver_pb2.SaverDef.V2:
      logging.warning("*******************************************************")
      logging.warning("TensorFlow's V1 checkpoint format has been deprecated.")
      logging.warning("Consider switching to the more efficient V2 format:")
      logging.warning("   `tf.train.Saver(write_version=tf.train.SaverDef.V2)`")
      logging.warning("now on by default.")
      logging.warning("*******************************************************")

    if os.path.split(latest_filename)[0]:
      raise ValueError("'latest_filename' must not contain path components")

    if global_step is not None:
      if not isinstance(global_step, compat.integral_types):
        global_step = training_util.global_step(sess, global_step)
      checkpoint_file = "%s-%d" % (save_path, global_step)
      if self._pad_step_number:
        # Zero-pads the step numbers, so that they are sorted when listed.
        checkpoint_file = "%s-%s" % (save_path, "{:08d}".format(global_step))
    else:
      checkpoint_file = save_path
      if os.path.basename(
          save_path) == latest_filename and not self._sharded:
        # Guard against collision between data file and checkpoint state file.
        raise ValueError(
            "'latest_filename' collides with 'save_path': '%s' and '%s'" %
            (latest_filename, save_path))

    if (not context.executing_eagerly() and
        not isinstance(sess, session.SessionInterface)):
      raise TypeError("'sess' must be a Session; %s" % sess)

    save_path_parent = os.path.dirname(save_path)
    if not self._is_empty:
      try:
        if context.executing_eagerly():
          self._build_eager(
              checkpoint_file, build_save=True, build_restore=False)
          model_checkpoint_path = self.saver_def.save_tensor_name
        else:
          model_checkpoint_path = sess.run(
              self.saver_def.save_tensor_name,
              {self.saver_def.filename_tensor_name: checkpoint_file})

        model_checkpoint_path = compat.as_str(model_checkpoint_path)
        if write_state:
          self._RecordLastCheckpoint(model_checkpoint_path)
          checkpoint_management.update_checkpoint_state_internal(
              save_dir=save_path_parent,
              model_checkpoint_path=model_checkpoint_path,
              all_model_checkpoint_paths=self.last_checkpoints,
              latest_filename=latest_filename,
              save_relative_paths=self._save_relative_paths)
          self._MaybeDeleteOldCheckpoints(meta_graph_suffix=meta_graph_suffix)
      except (errors.FailedPreconditionError, errors.NotFoundError) as exc:
        if not gfile.IsDirectory(save_path_parent):
          exc = ValueError(
              "Parent directory of {} doesn't exist, can't save.".format(
                  save_path))
        raise exc

    if write_meta_graph:
      meta_graph_filename = checkpoint_management.meta_graph_filename(
          checkpoint_file, meta_graph_suffix=meta_graph_suffix)
      if not context.executing_eagerly():
        with sess.graph.as_default():
          self.export_meta_graph(
              meta_graph_filename, strip_default_attrs=strip_default_attrs)

    if self._is_empty:
      return None
    else:
      return model_checkpoint_path

  def export_meta_graph(self,
                        filename=None,
                        collection_list=None,
                        as_text=False,
                        export_scope=None,
                        clear_devices=False,
                        clear_extraneous_savers=False,
                        strip_default_attrs=False):
    # pylint: disable=line-too-long
    """Writes `MetaGraphDef` to save_path/filename.

    Args:
      filename: Optional meta_graph filename including the path.
      collection_list: List of string keys to collect.
      as_text: If `True`, writes the meta_graph as an ASCII proto.
      export_scope: Optional `string`. Name scope to remove.
      clear_devices: Whether or not to clear the device field for an `Operation`
        or `Tensor` during export.
      clear_extraneous_savers: Remove any Saver-related information from the
        graph (both Save/Restore ops and SaverDefs) that are not associated
        with this Saver.
      strip_default_attrs: Boolean. If `True`, default-valued attributes will be
        removed from the NodeDefs. For a detailed guide, see
        [Stripping Default-Valued Attributes](https://github.com/tensorflow/tensorflow/blob/master/tensorflow/python/saved_model/README.md#stripping-default-valued-attributes).

    Returns:
      A `MetaGraphDef` proto.
    """
    # pylint: enable=line-too-long
    return export_meta_graph(
        filename=filename,
        graph_def=ops.get_default_graph().as_graph_def(add_shapes=True),
        saver_def=self.saver_def,
        collection_list=collection_list,
        as_text=as_text,
        export_scope=export_scope,
        clear_devices=clear_devices,
        clear_extraneous_savers=clear_extraneous_savers,
        strip_default_attrs=strip_default_attrs)

  def restore(self, sess, save_path):
    """Restores previously saved variables.

    This method runs the ops added by the constructor for restoring variables.
    It requires a session in which the graph was launched.  The variables to
    restore do not have to have been initialized, as restoring is itself a way
    to initialize variables.

    The `save_path` argument is typically a value previously returned from a
    `save()` call, or a call to `latest_checkpoint()`.

    Args:
      sess: A `Session` to use to restore the parameters. None in eager mode.
      save_path: Path where parameters were previously saved.

    Raises:
      ValueError: If save_path is None or not a valid checkpoint.
    """
    if self._is_empty:
      return
    if save_path is None:
      raise ValueError("Can't load save_path when it is None.")

    if not checkpoint_management.checkpoint_exists(compat.as_text(save_path)):
      raise ValueError("The passed save_path is not a valid checkpoint: "
                       + compat.as_text(save_path))

    logging.info("Restoring parameters from %s", compat.as_text(save_path))
    try:
      if context.executing_eagerly():
        self._build_eager(save_path, build_save=False, build_restore=True)
      else:
        sess.run(self.saver_def.restore_op_name,
                 {self.saver_def.filename_tensor_name: save_path})
    except errors.NotFoundError as err:
      # There are three common conditions that might cause this error:
      # 0. The file is missing. We ignore here, as this is checked above.
      # 1. This is an object-based checkpoint trying name-based loading.
      # 2. The graph has been altered and a variable or other name is missing.

      # 1. The checkpoint would not be loaded successfully as is. Try to parse
      # it as an object-based checkpoint.
      try:
        names_to_keys = object_graph_key_mapping(save_path)
      except errors.NotFoundError:
        # 2. This is not an object-based checkpoint, which likely means there
        # is a graph mismatch. Re-raise the original error with
        # a helpful message (b/110263146)
        raise _wrap_restore_error_with_msg(
            err, "a Variable name or other graph key that is missing")

      # This is an object-based checkpoint. We'll print a warning and then do
      # the restore.
      logging.warning(
          "Restoring an object-based checkpoint using a name-based saver. This "
          "may be somewhat fragile, and will re-build the Saver. Instead, "
          "consider loading object-based checkpoints using "
          "tf.train.Checkpoint().")
      self._object_restore_saver = saver_from_object_based_checkpoint(
          checkpoint_path=save_path,
          var_list=self._var_list,
          builder=self._builder,
          names_to_keys=names_to_keys,
          cached_saver=self._object_restore_saver)
      self._object_restore_saver.restore(sess=sess, save_path=save_path)
    except errors.InvalidArgumentError as err:
      # There is a mismatch between the graph and the checkpoint being loaded.
      # We add a more reasonable error message here to help users (b/110263146)
      raise _wrap_restore_error_with_msg(
          err, "a mismatch between the current graph and the graph")

  @staticmethod
  def _add_collection_def(meta_graph_def, key, export_scope=None):
    """Adds a collection to MetaGraphDef protocol buffer.

    Args:
      meta_graph_def: MetaGraphDef protocol buffer.
      key: One of the GraphKeys or user-defined string.
      export_scope: Optional `string`. Name scope to remove.
    """
    meta_graph.add_collection_def(meta_graph_def, key,
                                  export_scope=export_scope)


@tf_export("train.import_meta_graph")
def import_meta_graph(meta_graph_or_file, clear_devices=False,
                      import_scope=None, **kwargs):
  """Recreates a Graph saved in a `MetaGraphDef` proto.

  This function takes a `MetaGraphDef` protocol buffer as input. If
  the argument is a file containing a `MetaGraphDef` protocol buffer ,
  it constructs a protocol buffer from the file content. The function
  then adds all the nodes from the `graph_def` field to the
  current graph, recreates all the collections, and returns a saver
  constructed from the `saver_def` field.

  In combination with `export_meta_graph()`, this function can be used to

  * Serialize a graph along with other Python objects such as `QueueRunner`,
    `Variable` into a `MetaGraphDef`.

  * Restart training from a saved graph and checkpoints.

  * Run inference from a saved graph and checkpoints.

  ```Python
  ...
  # Create a saver.
  saver = tf.train.Saver(...variables...)
  # Remember the training_op we want to run by adding it to a collection.
  tf.add_to_collection('train_op', train_op)
  sess = tf.Session()
  for step in xrange(1000000):
      sess.run(train_op)
      if step % 1000 == 0:
          # Saves checkpoint, which by default also exports a meta_graph
          # named 'my-model-global_step.meta'.
          saver.save(sess, 'my-model', global_step=step)
  ```

  Later we can continue training from this saved `meta_graph` without building
  the model from scratch.

  ```Python
  with tf.Session() as sess:
    new_saver = tf.train.import_meta_graph('my-save-dir/my-model-10000.meta')
    new_saver.restore(sess, 'my-save-dir/my-model-10000')
    # tf.get_collection() returns a list. In this example we only want the
    # first one.
    train_op = tf.get_collection('train_op')[0]
    for step in xrange(1000000):
      sess.run(train_op)
  ```

  NOTE: Restarting training from saved `meta_graph` only works if the
  device assignments have not changed.

  Args:
    meta_graph_or_file: `MetaGraphDef` protocol buffer or filename (including
      the path) containing a `MetaGraphDef`.
    clear_devices: Whether or not to clear the device field for an `Operation`
      or `Tensor` during import.
    import_scope: Optional `string`. Name scope to add. Only used when
      initializing from protocol buffer.
    **kwargs: Optional keyed arguments.

  Returns:
    A saver constructed from `saver_def` in `MetaGraphDef` or None.

    A None value is returned if no variables exist in the `MetaGraphDef`
    (i.e., there are no variables to restore).

  Raises:
    RuntimeError: If called with eager execution enabled.

  @compatibility(eager)
  Exporting/importing meta graphs is not supported. No graph exists when eager
  execution is enabled.
  @end_compatibility
  """  # pylint: disable=g-doc-exception
  return _import_meta_graph_with_return_elements(
      meta_graph_or_file, clear_devices, import_scope, **kwargs)[0]


def _import_meta_graph_with_return_elements(
    meta_graph_or_file, clear_devices=False, import_scope=None,
    return_elements=None, **kwargs):
  """Import MetaGraph, and return both a saver and returned elements."""
  if context.executing_eagerly():
    raise RuntimeError("Exporting/importing meta graphs is not supported when "
                       "eager execution is enabled. No graph exists when eager "
                       "execution is enabled.")
  if not isinstance(meta_graph_or_file, meta_graph_pb2.MetaGraphDef):
    meta_graph_def = meta_graph.read_meta_graph_file(meta_graph_or_file)
  else:
    meta_graph_def = meta_graph_or_file

  imported_vars, imported_return_elements = (
      meta_graph.import_scoped_meta_graph_with_return_elements(
          meta_graph_def,
          clear_devices=clear_devices,
          import_scope=import_scope,
          return_elements=return_elements,
          **kwargs))

  saver = _create_saver_from_imported_meta_graph(
      meta_graph_def, import_scope, imported_vars)
  return saver, imported_return_elements


def _create_saver_from_imported_meta_graph(
    meta_graph_def, import_scope, imported_vars):
  """Return a saver for restoring variable values to an imported MetaGraph."""
  if meta_graph_def.HasField("saver_def"):
    # Infer the scope that is prepended by `import_scoped_meta_graph`.
    scope = import_scope
    var_names = list(imported_vars.keys())
    if var_names:
      sample_key = var_names[0]
      sample_var = imported_vars[sample_key]
      scope = sample_var.name[:-len(sample_key)]

    return Saver(saver_def=meta_graph_def.saver_def, name=scope)
  else:
    if variables._all_saveable_objects(scope=import_scope):  # pylint: disable=protected-access
      # Return the default saver instance for all graph variables.
      return Saver()
    else:
      # If no graph variables exist, then a Saver cannot be constructed.
      logging.info("Saver not created because there are no variables in the"
                   " graph to restore")
      return None


@tf_export("train.export_meta_graph")
def export_meta_graph(filename=None,
                      meta_info_def=None,
                      graph_def=None,
                      saver_def=None,
                      collection_list=None,
                      as_text=False,
                      graph=None,
                      export_scope=None,
                      clear_devices=False,
                      clear_extraneous_savers=False,
                      strip_default_attrs=False,
                      **kwargs):
  # pylint: disable=line-too-long
  """Returns `MetaGraphDef` proto. Optionally writes it to filename.

  This function exports the graph, saver, and collection objects into
  `MetaGraphDef` protocol buffer with the intention of it being imported
  at a later time or location to restart training, run inference, or be
  a subgraph.

  Args:
    filename: Optional filename including the path for writing the
      generated `MetaGraphDef` protocol buffer.
    meta_info_def: `MetaInfoDef` protocol buffer.
    graph_def: `GraphDef` protocol buffer.
    saver_def: `SaverDef` protocol buffer.
    collection_list: List of string keys to collect.
    as_text: If `True`, writes the `MetaGraphDef` as an ASCII proto.
    graph: The `Graph` to export. If `None`, use the default graph.
    export_scope: Optional `string`. Name scope under which to extract
      the subgraph. The scope name will be striped from the node definitions
      for easy import later into new name scopes. If `None`, the whole graph
      is exported. graph_def and export_scope cannot both be specified.
    clear_devices: Whether or not to clear the device field for an `Operation`
      or `Tensor` during export.
    clear_extraneous_savers: Remove any Saver-related information from the
        graph (both Save/Restore ops and SaverDefs) that are not associated
        with the provided SaverDef.
    strip_default_attrs: Boolean. If `True`, default-valued attributes will be
      removed from the NodeDefs. For a detailed guide, see
      [Stripping Default-Valued Attributes](https://github.com/tensorflow/tensorflow/blob/master/tensorflow/python/saved_model/README.md#stripping-default-valued-attributes).
    **kwargs: Optional keyed arguments.

  Returns:
    A `MetaGraphDef` proto.

  Raises:
    ValueError: When the `GraphDef` is larger than 2GB.
    RuntimeError: If called with eager execution enabled.

  @compatibility(eager)
  Exporting/importing meta graphs is not supported. No graph exists when eager
  execution is enabled.
  @end_compatibility
  """
  # pylint: enable=line-too-long
  if context.executing_eagerly():
    raise RuntimeError("Exporting/importing meta graphs is not supported when "
                       "eager execution is enabled. No graph exists when eager "
                       "execution is enabled.")
  meta_graph_def, _ = meta_graph.export_scoped_meta_graph(
      filename=filename,
      meta_info_def=meta_info_def,
      graph_def=graph_def,
      saver_def=saver_def,
      collection_list=collection_list,
      as_text=as_text,
      graph=graph,
      export_scope=export_scope,
      clear_devices=clear_devices,
      clear_extraneous_savers=clear_extraneous_savers,
      strip_default_attrs=strip_default_attrs,
      **kwargs)
  return meta_graph_def


def _wrap_restore_error_with_msg(err, extra_verbiage):
  err_msg = ("Restoring from checkpoint failed. This is most likely "
             "due to {} from the checkpoint. Please ensure that you "
             "have not altered the graph expected based on the checkpoint. "
             "Original error:\n\n{}").format(extra_verbiage, err.message)
  return err.__class__(err.node_def, err.op, err_msg)


ops.register_proto_function(
    ops.GraphKeys.SAVERS,
    proto_type=saver_pb2.SaverDef,
    to_proto=Saver.to_proto,
    from_proto=Saver.from_proto)


def object_graph_key_mapping(checkpoint_path):
  """Return name to key mappings from the checkpoint.

  Args:
    checkpoint_path: string, path to object-based checkpoint

  Returns:
    Dictionary mapping tensor names to checkpoint keys.
  """
  reader = pywrap_tensorflow.NewCheckpointReader(checkpoint_path)
  object_graph_string = reader.get_tensor(
      checkpointable.OBJECT_GRAPH_PROTO_KEY)
  object_graph_proto = (
      checkpointable_object_graph_pb2.CheckpointableObjectGraph())
  object_graph_proto.ParseFromString(object_graph_string)
  names_to_keys = {}
  for node in object_graph_proto.nodes:
    for attribute in node.attributes:
      names_to_keys[attribute.full_name] = attribute.checkpoint_key
  return names_to_keys


def saver_from_object_based_checkpoint(
    checkpoint_path, var_list=None, builder=None, names_to_keys=None,
    cached_saver=None):
  """Return a `Saver` which reads from an object-based checkpoint.

  This function validates that all variables in the variables list are remapped
  in the object-based checkpoint (or `names_to_keys` dict if provided). A
  saver will be created with the list of remapped variables.

  The `cached_saver` argument allows the user to pass in a previously created
  saver, so multiple `saver.restore()` calls don't pollute the graph when graph
  building. This assumes that keys are consistent, meaning that the
    1) `checkpoint_path` checkpoint, and
    2) checkpoint used to create the `cached_saver`
  are the same type of object-based checkpoint. If this argument is set, this
  function will simply validate that all variables have been remapped by the
  checkpoint at `checkpoint_path`.

  Note that in general, `tf.train.Checkpoint` should be used to restore/save an
  object-based checkpoint.

  Args:
    checkpoint_path: string, path to object-based checkpoint
    var_list: list of `Variables` that appear in the checkpoint. If `None`,
      `var_list` will be set to all saveable objects.
    builder: a `BaseSaverBuilder` instance. If `None`, a new `BulkSaverBuilder`
      will be created.
    names_to_keys: dict mapping string tensor names to checkpooint keys. If
      `None`, this dict will be generated from the checkpoint file.
    cached_saver: Cached `Saver` object with remapped variables.

  Returns:
    `Saver` with remapped variables for reading from an object-based checkpoint.

  Raises:
    ValueError if the checkpoint provided is not an object-based checkpoint.
    NotFoundError: If one of the variables in `var_list` can not be found in the
      checkpoint. This could mean the checkpoint or `names_to_keys` mapping is
      missing the variable.
  """
  if names_to_keys is None:
    try:
      names_to_keys = object_graph_key_mapping(checkpoint_path)
    except errors.NotFoundError:
      raise ValueError("Checkpoint in %s not an object-based checkpoint."
                       % checkpoint_path)
  if var_list is None:
    var_list = variables._all_saveable_objects()  # pylint: disable=protected-access
  if builder is None:
    builder = BulkSaverBuilder()

  saveables = builder._ValidateAndSliceInputs(var_list)  # pylint: disable=protected-access
  for saveable in saveables:
    for spec in saveable.specs:
      if spec.name not in names_to_keys:
        raise errors.NotFoundError(
            None, None,
            message=("Attempting to load an object-based checkpoint using "
                     "variable names, but could not find %s in the "
                     "checkpoint.") % spec.name)
      spec.name = names_to_keys[spec.name]

  if cached_saver is None:
    return Saver(saveables)
  return cached_saver
