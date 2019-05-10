# Copyright 2019 The TensorFlow Authors. All Rights Reserved.
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
"""Tests for custom training loops."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from absl.testing import parameterized

from tensorflow.python import keras
from tensorflow.python.eager import backprop
from tensorflow.python.eager import def_function
from tensorflow.python.framework import ops
from tensorflow.python.keras import keras_parameterized
from tensorflow.python.keras import testing_utils
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import math_ops
from tensorflow.python.platform import test


class LayerWithLosses(keras.layers.Layer):

  def build(self, input_shape):
    self.v = self.add_weight(
        name='hey',
        shape=(),
        initializer='ones',
        regularizer=keras.regularizers.l1(100))

  def call(self, inputs):
    self.add_loss(math_ops.reduce_sum(inputs))
    return self.v * inputs


def add_loss_step(defun):
  optimizer = keras.optimizer_v2.adam.Adam()
  model = testing_utils.get_model_from_layers([LayerWithLosses()],
                                              input_shape=(10,))

  def train_step(x):
    with backprop.GradientTape() as tape:
      model(x)
      assert len(model.losses) == 2
      loss = math_ops.reduce_sum(model.losses)
    gradients = tape.gradient(loss, model.trainable_weights)
    optimizer.apply_gradients(zip(gradients, model.trainable_weights))
    return loss

  if defun:
    train_step = def_function.function(train_step)

  x = array_ops.ones((10, 10))
  return train_step(x)


def batch_norm_step(defun):
  optimizer = keras.optimizer_v2.adadelta.Adadelta()
  model = testing_utils.get_model_from_layers([
      keras.layers.BatchNormalization(momentum=0.9),
      keras.layers.Dense(1, kernel_initializer='zeros', activation='softmax')
  ],
                                              input_shape=(10,))

  def train_step(x, y):
    with backprop.GradientTape() as tape:
      y_pred = model(x, training=True)
      loss = keras.losses.binary_crossentropy(y, y_pred)
    gradients = tape.gradient(loss, model.trainable_weights)
    optimizer.apply_gradients(zip(gradients, model.trainable_weights))
    return loss, model(x, training=False)

  if defun:
    train_step = def_function.function(train_step)

  x, y = array_ops.ones((10, 10)), array_ops.ones((10, 1))
  return train_step(x, y)


@keras_parameterized.run_with_all_model_types
class CustomTrainingLoopTest(keras_parameterized.TestCase):

  @parameterized.named_parameters(('add_loss_step', add_loss_step),
                                  ('batch_norm_step', batch_norm_step))
  def test_eager_and_tf_function(self, train_step):
    eager_result = train_step(defun=False)
    fn_result = train_step(defun=True)
    self.assertAllClose(eager_result, fn_result)


if __name__ == '__main__':
  ops.enable_eager_execution()
  test.main()
