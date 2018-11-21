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
# pylint: disable=line-too-long
"""List of renames to apply when converting from TF 1.0 to TF 2.0.

THIS FILE IS AUTOGENERATED: To update, please run:
  bazel build tensorflow/tools/compatibility/update:generate_v2_renames_map
  bazel-bin/tensorflow/tools/compatibility/update/generate_v2_renames_map
This file should be updated whenever endpoints are deprecated.
"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

renames = {
    'tf.AUTO_REUSE': 'tf.compat.v1.AUTO_REUSE',
    'tf.COMPILER_VERSION': 'tf.version.COMPILER_VERSION',
    'tf.CXX11_ABI_FLAG': 'tf.sysconfig.CXX11_ABI_FLAG',
    'tf.ConditionalAccumulator': 'tf.compat.v1.ConditionalAccumulator',
    'tf.ConditionalAccumulatorBase': 'tf.compat.v1.ConditionalAccumulatorBase',
    'tf.DeviceSpec': 'tf.compat.v1.DeviceSpec',
    'tf.Dimension': 'tf.compat.v1.Dimension',
    'tf.FixedLenFeature': 'tf.io.FixedLenFeature',
    'tf.FixedLenSequenceFeature': 'tf.io.FixedLenSequenceFeature',
    'tf.FixedLengthRecordReader': 'tf.compat.v1.FixedLengthRecordReader',
    'tf.GIT_VERSION': 'tf.version.GIT_VERSION',
    'tf.GRAPH_DEF_VERSION': 'tf.version.GRAPH_DEF_VERSION',
    'tf.GRAPH_DEF_VERSION_MIN_CONSUMER': 'tf.version.GRAPH_DEF_VERSION_MIN_CONSUMER',
    'tf.GRAPH_DEF_VERSION_MIN_PRODUCER': 'tf.version.GRAPH_DEF_VERSION_MIN_PRODUCER',
    'tf.IdentityReader': 'tf.compat.v1.IdentityReader',
    'tf.InteractiveSession': 'tf.compat.v1.InteractiveSession',
    'tf.LMDBReader': 'tf.compat.v1.LMDBReader',
    'tf.MONOLITHIC_BUILD': 'tf.sysconfig.MONOLITHIC_BUILD',
    'tf.NoGradient': 'tf.no_gradient',
    'tf.NotDifferentiable': 'tf.no_gradient',
    'tf.OpError': 'tf.errors.OpError',
    'tf.PaddingFIFOQueue': 'tf.io.PaddingFIFOQueue',
    'tf.Print': 'tf.compat.v1.Print',
    'tf.PriorityQueue': 'tf.io.PriorityQueue',
    'tf.QUANTIZED_DTYPES': 'tf.dtypes.QUANTIZED_DTYPES',
    'tf.QueueBase': 'tf.io.QueueBase',
    'tf.RandomShuffleQueue': 'tf.io.RandomShuffleQueue',
    'tf.ReaderBase': 'tf.compat.v1.ReaderBase',
    'tf.Session': 'tf.compat.v1.Session',
    'tf.SparseConditionalAccumulator': 'tf.sparse.SparseConditionalAccumulator',
    'tf.SparseFeature': 'tf.io.SparseFeature',
    'tf.TFRecordReader': 'tf.compat.v1.TFRecordReader',
    'tf.TensorInfo': 'tf.compat.v1.TensorInfo',
    'tf.TextLineReader': 'tf.compat.v1.TextLineReader',
    'tf.VERSION': 'tf.version.VERSION',
    'tf.VarLenFeature': 'tf.io.VarLenFeature',
    'tf.VariableScope': 'tf.compat.v1.VariableScope',
    'tf.WholeFileReader': 'tf.compat.v1.WholeFileReader',
    'tf.accumulate_n': 'tf.math.accumulate_n',
    'tf.add_check_numerics_ops': 'tf.compat.v1.add_check_numerics_ops',
    'tf.add_to_collection': 'tf.compat.v1.add_to_collection',
    'tf.add_to_collections': 'tf.compat.v1.add_to_collections',
    'tf.all_variables': 'tf.compat.v1.all_variables',
    'tf.angle': 'tf.math.angle',
    'tf.app.run': 'tf.compat.v1.app.run',
    'tf.arg_max': 'tf.compat.v1.arg_max',
    'tf.arg_min': 'tf.compat.v1.arg_min',
    'tf.assert_greater_equal': 'tf.compat.v1.assert_greater_equal',
    'tf.assert_integer': 'tf.compat.v1.assert_integer',
    'tf.assert_less_equal': 'tf.compat.v1.assert_less_equal',
    'tf.assert_near': 'tf.compat.v1.assert_near',
    'tf.assert_negative': 'tf.compat.v1.assert_negative',
    'tf.assert_non_negative': 'tf.compat.v1.assert_non_negative',
    'tf.assert_non_positive': 'tf.compat.v1.assert_non_positive',
    'tf.assert_none_equal': 'tf.compat.v1.assert_none_equal',
    'tf.assert_positive': 'tf.compat.v1.assert_positive',
    'tf.assert_proper_iterable': 'tf.debugging.assert_proper_iterable',
    'tf.assert_rank_at_least': 'tf.compat.v1.assert_rank_at_least',
    'tf.assert_rank_in': 'tf.compat.v1.assert_rank_in',
    'tf.assert_same_float_dtype': 'tf.debugging.assert_same_float_dtype',
    'tf.assert_scalar': 'tf.compat.v1.assert_scalar',
    'tf.assert_type': 'tf.compat.v1.assert_type',
    'tf.assert_variables_initialized': 'tf.compat.v1.assert_variables_initialized',
    'tf.assign': 'tf.compat.v1.assign',
    'tf.assign_add': 'tf.compat.v1.assign_add',
    'tf.assign_sub': 'tf.compat.v1.assign_sub',
    'tf.betainc': 'tf.math.betainc',
    'tf.bincount': 'tf.math.bincount',
    'tf.ceil': 'tf.math.ceil',
    'tf.check_numerics': 'tf.debugging.check_numerics',
    'tf.cholesky': 'tf.linalg.cholesky',
    'tf.cholesky_solve': 'tf.linalg.cholesky_solve',
    'tf.colocate_with': 'tf.compat.v1.colocate_with',
    'tf.confusion_matrix': 'tf.math.confusion_matrix',
    'tf.conj': 'tf.math.conj',
    'tf.container': 'tf.compat.v1.container',
    'tf.convert_to_tensor_or_indexed_slices': 'tf.compat.v1.convert_to_tensor_or_indexed_slices',
    'tf.convert_to_tensor_or_sparse_tensor': 'tf.compat.v1.convert_to_tensor_or_sparse_tensor',
    'tf.count_nonzero': 'tf.compat.v1.count_nonzero',
    'tf.count_up_to': 'tf.compat.v1.count_up_to',
    'tf.cross': 'tf.linalg.cross',
    'tf.cumprod': 'tf.math.cumprod',
    'tf.debugging.is_finite': 'tf.math.is_finite',
    'tf.debugging.is_inf': 'tf.math.is_inf',
    'tf.debugging.is_nan': 'tf.math.is_nan',
    'tf.debugging.is_non_decreasing': 'tf.math.is_non_decreasing',
    'tf.debugging.is_strictly_increasing': 'tf.math.is_strictly_increasing',
    'tf.decode_base64': 'tf.io.decode_base64',
    'tf.decode_compressed': 'tf.io.decode_compressed',
    'tf.decode_csv': 'tf.io.decode_csv',
    'tf.decode_json_example': 'tf.io.decode_json_example',
    'tf.decode_raw': 'tf.io.decode_raw',
    'tf.delete_session_tensor': 'tf.compat.v1.delete_session_tensor',
    'tf.depth_to_space': 'tf.nn.depth_to_space',
    'tf.dequantize': 'tf.quantization.dequantize',
    'tf.deserialize_many_sparse': 'tf.io.deserialize_many_sparse',
    'tf.diag': 'tf.linalg.tensor_diag',
    'tf.diag_part': 'tf.linalg.tensor_diag_part',
    'tf.digamma': 'tf.math.digamma',
    'tf.dimension_at_index': 'tf.compat.v1.dimension_at_index',
    'tf.dimension_value': 'tf.compat.v1.dimension_value',
    'tf.disable_resource_variables': 'tf.compat.v1.disable_resource_variables',
    'tf.disable_v2_tensorshape': 'tf.compat.v1.disable_v2_tensorshape',
    'tf.distributions.Bernoulli': 'tf.compat.v1.distributions.Bernoulli',
    'tf.distributions.Beta': 'tf.compat.v1.distributions.Beta',
    'tf.distributions.Categorical': 'tf.compat.v1.distributions.Categorical',
    'tf.distributions.Dirichlet': 'tf.compat.v1.distributions.Dirichlet',
    'tf.distributions.DirichletMultinomial': 'tf.compat.v1.distributions.DirichletMultinomial',
    'tf.distributions.Distribution': 'tf.compat.v1.distributions.Distribution',
    'tf.distributions.Exponential': 'tf.compat.v1.distributions.Exponential',
    'tf.distributions.FULLY_REPARAMETERIZED': 'tf.compat.v1.distributions.FULLY_REPARAMETERIZED',
    'tf.distributions.Gamma': 'tf.compat.v1.distributions.Gamma',
    'tf.distributions.Laplace': 'tf.compat.v1.distributions.Laplace',
    'tf.distributions.Multinomial': 'tf.compat.v1.distributions.Multinomial',
    'tf.distributions.NOT_REPARAMETERIZED': 'tf.compat.v1.distributions.NOT_REPARAMETERIZED',
    'tf.distributions.Normal': 'tf.compat.v1.distributions.Normal',
    'tf.distributions.RegisterKL': 'tf.compat.v1.distributions.RegisterKL',
    'tf.distributions.ReparameterizationType': 'tf.compat.v1.distributions.ReparameterizationType',
    'tf.distributions.StudentT': 'tf.compat.v1.distributions.StudentT',
    'tf.distributions.Uniform': 'tf.compat.v1.distributions.Uniform',
    'tf.distributions.kl_divergence': 'tf.compat.v1.distributions.kl_divergence',
    'tf.div': 'tf.compat.v1.div',
    'tf.enable_resource_variables': 'tf.compat.v1.enable_resource_variables',
    'tf.enable_v2_tensorshape': 'tf.compat.v1.enable_v2_tensorshape',
    'tf.encode_base64': 'tf.io.encode_base64',
    'tf.erf': 'tf.math.erf',
    'tf.erfc': 'tf.math.erfc',
    'tf.expm1': 'tf.math.expm1',
    'tf.extract_image_patches': 'tf.image.extract_image_patches',
    'tf.fake_quant_with_min_max_args': 'tf.quantization.fake_quant_with_min_max_args',
    'tf.fake_quant_with_min_max_args_gradient': 'tf.quantization.fake_quant_with_min_max_args_gradient',
    'tf.fake_quant_with_min_max_vars': 'tf.quantization.fake_quant_with_min_max_vars',
    'tf.fake_quant_with_min_max_vars_gradient': 'tf.quantization.fake_quant_with_min_max_vars_gradient',
    'tf.fake_quant_with_min_max_vars_per_channel': 'tf.quantization.fake_quant_with_min_max_vars_per_channel',
    'tf.fake_quant_with_min_max_vars_per_channel_gradient': 'tf.quantization.fake_quant_with_min_max_vars_per_channel_gradient',
    'tf.feature_column.input_layer': 'tf.compat.v1.feature_column.input_layer',
    'tf.feature_column.linear_model': 'tf.compat.v1.feature_column.linear_model',
    'tf.fft': 'tf.signal.fft',
    'tf.fft2d': 'tf.signal.fft2d',
    'tf.fft3d': 'tf.signal.fft3d',
    'tf.floordiv': 'tf.math.floordiv',
    'tf.get_collection': 'tf.compat.v1.get_collection',
    'tf.get_collection_ref': 'tf.compat.v1.get_collection_ref',
    'tf.get_default_graph': 'tf.compat.v1.get_default_graph',
    'tf.get_default_session': 'tf.compat.v1.get_default_session',
    'tf.get_local_variable': 'tf.compat.v1.get_local_variable',
    'tf.get_seed': 'tf.compat.v1.get_seed',
    'tf.get_session_handle': 'tf.compat.v1.get_session_handle',
    'tf.get_session_tensor': 'tf.compat.v1.get_session_tensor',
    'tf.get_variable': 'tf.compat.v1.get_variable',
    'tf.get_variable_scope': 'tf.compat.v1.get_variable_scope',
    'tf.gfile.Exists': 'tf.compat.v1.gfile.Exists',
    'tf.gfile.FastGFile': 'tf.compat.v1.gfile.FastGFile',
    'tf.gfile.GFile': 'tf.compat.v1.gfile.GFile',
    'tf.gfile.Open': 'tf.compat.v1.gfile.Open',
    'tf.global_norm': 'tf.linalg.global_norm',
    'tf.global_variables': 'tf.compat.v1.global_variables',
    'tf.global_variables_initializer': 'tf.compat.v1.global_variables_initializer',
    'tf.glorot_normal_initializer': 'tf.keras.initializers.glorot_normal',
    'tf.graph_util.convert_variables_to_constants': 'tf.compat.v1.graph_util.convert_variables_to_constants',
    'tf.graph_util.extract_sub_graph': 'tf.compat.v1.graph_util.extract_sub_graph',
    'tf.graph_util.must_run_on_cpu': 'tf.compat.v1.graph_util.must_run_on_cpu',
    'tf.graph_util.remove_training_nodes': 'tf.compat.v1.graph_util.remove_training_nodes',
    'tf.graph_util.tensor_shape_from_node_def_name': 'tf.compat.v1.graph_util.tensor_shape_from_node_def_name',
    'tf.ifft': 'tf.signal.ifft',
    'tf.ifft2d': 'tf.signal.ifft2d',
    'tf.ifft3d': 'tf.signal.ifft3d',
    'tf.igamma': 'tf.math.igamma',
    'tf.igammac': 'tf.math.igammac',
    'tf.imag': 'tf.math.imag',
    'tf.image.resize_area': 'tf.compat.v1.image.resize_area',
    'tf.image.resize_bicubic': 'tf.compat.v1.image.resize_bicubic',
    'tf.image.resize_bilinear': 'tf.compat.v1.image.resize_bilinear',
    'tf.image.resize_nearest_neighbor': 'tf.compat.v1.image.resize_nearest_neighbor',
    'tf.initialize_all_tables': 'tf.compat.v1.initialize_all_tables',
    'tf.initialize_all_variables': 'tf.compat.v1.initialize_all_variables',
    'tf.initialize_local_variables': 'tf.compat.v1.initialize_local_variables',
    'tf.initialize_variables': 'tf.compat.v1.initialize_variables',
    'tf.initializers.global_variables': 'tf.compat.v1.initializers.global_variables',
    'tf.initializers.local_variables': 'tf.compat.v1.initializers.local_variables',
    'tf.initializers.tables_initializer': 'tf.compat.v1.initializers.tables_initializer',
    'tf.initializers.variables': 'tf.compat.v1.initializers.variables',
    'tf.invert_permutation': 'tf.math.invert_permutation',
    'tf.is_finite': 'tf.math.is_finite',
    'tf.is_inf': 'tf.math.is_inf',
    'tf.is_nan': 'tf.math.is_nan',
    'tf.is_non_decreasing': 'tf.math.is_non_decreasing',
    'tf.is_numeric_tensor': 'tf.debugging.is_numeric_tensor',
    'tf.is_strictly_increasing': 'tf.math.is_strictly_increasing',
    'tf.is_variable_initialized': 'tf.compat.v1.is_variable_initialized',
    'tf.keras.backend.get_session': 'tf.compat.v1.keras.backend.get_session',
    'tf.layers.AveragePooling1D': 'tf.compat.v1.layers.AveragePooling1D',
    'tf.layers.AveragePooling2D': 'tf.compat.v1.layers.AveragePooling2D',
    'tf.layers.AveragePooling3D': 'tf.compat.v1.layers.AveragePooling3D',
    'tf.layers.BatchNormalization': 'tf.compat.v1.layers.BatchNormalization',
    'tf.layers.Conv1D': 'tf.compat.v1.layers.Conv1D',
    'tf.layers.Conv2D': 'tf.compat.v1.layers.Conv2D',
    'tf.layers.Conv2DTranspose': 'tf.compat.v1.layers.Conv2DTranspose',
    'tf.layers.Conv3D': 'tf.compat.v1.layers.Conv3D',
    'tf.layers.Conv3DTranspose': 'tf.compat.v1.layers.Conv3DTranspose',
    'tf.layers.Dense': 'tf.compat.v1.layers.Dense',
    'tf.layers.Dropout': 'tf.compat.v1.layers.Dropout',
    'tf.layers.Flatten': 'tf.compat.v1.layers.Flatten',
    'tf.layers.InputSpec': 'tf.keras.layers.InputSpec',
    'tf.layers.Layer': 'tf.compat.v1.layers.Layer',
    'tf.layers.MaxPooling1D': 'tf.compat.v1.layers.MaxPooling1D',
    'tf.layers.MaxPooling2D': 'tf.compat.v1.layers.MaxPooling2D',
    'tf.layers.MaxPooling3D': 'tf.compat.v1.layers.MaxPooling3D',
    'tf.layers.SeparableConv1D': 'tf.compat.v1.layers.SeparableConv1D',
    'tf.layers.SeparableConv2D': 'tf.compat.v1.layers.SeparableConv2D',
    'tf.layers.average_pooling1d': 'tf.compat.v1.layers.average_pooling1d',
    'tf.layers.average_pooling2d': 'tf.compat.v1.layers.average_pooling2d',
    'tf.layers.average_pooling3d': 'tf.compat.v1.layers.average_pooling3d',
    'tf.layers.batch_normalization': 'tf.compat.v1.layers.batch_normalization',
    'tf.layers.conv1d': 'tf.compat.v1.layers.conv1d',
    'tf.layers.conv2d': 'tf.compat.v1.layers.conv2d',
    'tf.layers.conv2d_transpose': 'tf.compat.v1.layers.conv2d_transpose',
    'tf.layers.conv3d': 'tf.compat.v1.layers.conv3d',
    'tf.layers.conv3d_transpose': 'tf.compat.v1.layers.conv3d_transpose',
    'tf.layers.dense': 'tf.compat.v1.layers.dense',
    'tf.layers.dropout': 'tf.compat.v1.layers.dropout',
    'tf.layers.experimental.keras_style_scope': 'tf.compat.v1.layers.experimental.keras_style_scope',
    'tf.layers.experimental.set_keras_style': 'tf.compat.v1.layers.experimental.set_keras_style',
    'tf.layers.flatten': 'tf.compat.v1.layers.flatten',
    'tf.layers.max_pooling1d': 'tf.compat.v1.layers.max_pooling1d',
    'tf.layers.max_pooling2d': 'tf.compat.v1.layers.max_pooling2d',
    'tf.layers.max_pooling3d': 'tf.compat.v1.layers.max_pooling3d',
    'tf.layers.separable_conv1d': 'tf.compat.v1.layers.separable_conv1d',
    'tf.layers.separable_conv2d': 'tf.compat.v1.layers.separable_conv2d',
    'tf.lbeta': 'tf.math.lbeta',
    'tf.lgamma': 'tf.math.lgamma',
    'tf.load_file_system_library': 'tf.compat.v1.load_file_system_library',
    'tf.local_variables': 'tf.compat.v1.local_variables',
    'tf.local_variables_initializer': 'tf.compat.v1.local_variables_initializer',
    'tf.log_sigmoid': 'tf.math.log_sigmoid',
    'tf.logging.DEBUG': 'tf.compat.v1.logging.DEBUG',
    'tf.logging.ERROR': 'tf.compat.v1.logging.ERROR',
    'tf.logging.FATAL': 'tf.compat.v1.logging.FATAL',
    'tf.logging.INFO': 'tf.compat.v1.logging.INFO',
    'tf.logging.TaskLevelStatusMessage': 'tf.compat.v1.logging.TaskLevelStatusMessage',
    'tf.logging.WARN': 'tf.compat.v1.logging.WARN',
    'tf.logging.debug': 'tf.compat.v1.logging.debug',
    'tf.logging.error': 'tf.compat.v1.logging.error',
    'tf.logging.fatal': 'tf.compat.v1.logging.fatal',
    'tf.logging.flush': 'tf.compat.v1.logging.flush',
    'tf.logging.get_verbosity': 'tf.compat.v1.logging.get_verbosity',
    'tf.logging.info': 'tf.compat.v1.logging.info',
    'tf.logging.log': 'tf.compat.v1.logging.log',
    'tf.logging.log_every_n': 'tf.compat.v1.logging.log_every_n',
    'tf.logging.log_first_n': 'tf.compat.v1.logging.log_first_n',
    'tf.logging.log_if': 'tf.compat.v1.logging.log_if',
    'tf.logging.set_verbosity': 'tf.compat.v1.logging.set_verbosity',
    'tf.logging.vlog': 'tf.compat.v1.logging.vlog',
    'tf.logging.warn': 'tf.compat.v1.logging.warn',
    'tf.logging.warning': 'tf.compat.v1.logging.warning',
    'tf.logical_xor': 'tf.math.logical_xor',
    'tf.make_template': 'tf.compat.v1.make_template',
    'tf.make_tensor_proto': 'tf.compat.v1.make_tensor_proto',
    'tf.manip.batch_to_space_nd': 'tf.batch_to_space_nd',
    'tf.manip.gather_nd': 'tf.gather_nd',
    'tf.manip.reshape': 'tf.reshape',
    'tf.manip.reverse': 'tf.reverse',
    'tf.manip.roll': 'tf.roll',
    'tf.manip.scatter_nd': 'tf.scatter_nd',
    'tf.manip.space_to_batch_nd': 'tf.space_to_batch_nd',
    'tf.manip.tile': 'tf.tile',
    'tf.matching_files': 'tf.io.matching_files',
    'tf.matrix_band_part': 'tf.linalg.band_part',
    'tf.matrix_determinant': 'tf.linalg.det',
    'tf.matrix_diag': 'tf.linalg.diag',
    'tf.matrix_diag_part': 'tf.linalg.diag_part',
    'tf.matrix_inverse': 'tf.linalg.inv',
    'tf.matrix_set_diag': 'tf.linalg.set_diag',
    'tf.matrix_solve': 'tf.linalg.solve',
    'tf.matrix_solve_ls': 'tf.linalg.lstsq',
    'tf.matrix_transpose': 'tf.linalg.transpose',
    'tf.matrix_triangular_solve': 'tf.linalg.triangular_solve',
    'tf.model_variables': 'tf.compat.v1.model_variables',
    'tf.moving_average_variables': 'tf.compat.v1.moving_average_variables',
    'tf.multinomial': 'tf.compat.v1.multinomial',
    'tf.nn.conv3d_backprop_filter_v2': 'tf.nn.conv3d_backprop_filter',
    'tf.nn.ctc_beam_search_decoder_v2': 'tf.nn.ctc_beam_search_decoder',
    'tf.nn.depthwise_conv2d_native': 'tf.compat.v1.nn.depthwise_conv2d_native',
    'tf.nn.depthwise_conv2d_native_backprop_filter': 'tf.nn.depthwise_conv2d_backprop_filter',
    'tf.nn.depthwise_conv2d_native_backprop_input': 'tf.nn.depthwise_conv2d_backprop_input',
    'tf.nn.dynamic_rnn': 'tf.compat.v1.nn.dynamic_rnn',
    'tf.nn.log_uniform_candidate_sampler': 'tf.random.log_uniform_candidate_sampler',
    'tf.nn.quantized_avg_pool': 'tf.compat.v1.nn.quantized_avg_pool',
    'tf.nn.quantized_conv2d': 'tf.compat.v1.nn.quantized_conv2d',
    'tf.nn.quantized_max_pool': 'tf.compat.v1.nn.quantized_max_pool',
    'tf.nn.quantized_relu_x': 'tf.compat.v1.nn.quantized_relu_x',
    'tf.nn.raw_rnn': 'tf.compat.v1.nn.raw_rnn',
    'tf.nn.rnn_cell.BasicLSTMCell': 'tf.compat.v1.nn.rnn_cell.BasicLSTMCell',
    'tf.nn.rnn_cell.BasicRNNCell': 'tf.compat.v1.nn.rnn_cell.BasicRNNCell',
    'tf.nn.rnn_cell.GRUCell': 'tf.compat.v1.nn.rnn_cell.GRUCell',
    'tf.nn.rnn_cell.LSTMCell': 'tf.compat.v1.nn.rnn_cell.LSTMCell',
    'tf.nn.softmax_cross_entropy_with_logits_v2': 'tf.nn.softmax_cross_entropy_with_logits',
    'tf.nn.static_rnn': 'tf.compat.v1.nn.static_rnn',
    'tf.nn.uniform_candidate_sampler': 'tf.random.uniform_candidate_sampler',
    'tf.nn.xw_plus_b': 'tf.compat.v1.nn.xw_plus_b',
    'tf.op_scope': 'tf.compat.v1.op_scope',
    'tf.orthogonal_initializer': 'tf.keras.initializers.Orthogonal',
    'tf.parse_example': 'tf.io.parse_example',
    'tf.parse_single_example': 'tf.io.parse_single_example',
    'tf.parse_single_sequence_example': 'tf.io.parse_single_sequence_example',
    'tf.parse_tensor': 'tf.io.parse_tensor',
    'tf.placeholder': 'tf.compat.v1.placeholder',
    'tf.placeholder_with_default': 'tf.compat.v1.placeholder_with_default',
    'tf.polygamma': 'tf.math.polygamma',
    'tf.profiler.AdviceProto': 'tf.compat.v1.profiler.AdviceProto',
    'tf.profiler.GraphNodeProto': 'tf.compat.v1.profiler.GraphNodeProto',
    'tf.profiler.MultiGraphNodeProto': 'tf.compat.v1.profiler.MultiGraphNodeProto',
    'tf.profiler.OpLogProto': 'tf.compat.v1.profiler.OpLogProto',
    'tf.profiler.ProfileOptionBuilder': 'tf.compat.v1.profiler.ProfileOptionBuilder',
    'tf.profiler.Profiler': 'tf.compat.v1.profiler.Profiler',
    'tf.profiler.advise': 'tf.compat.v1.profiler.advise',
    'tf.profiler.profile': 'tf.compat.v1.profiler.profile',
    'tf.profiler.write_op_log': 'tf.compat.v1.profiler.write_op_log',
    'tf.py_func': 'tf.compat.v1.py_func',
    'tf.python_io.TFRecordCompressionType': 'tf.io.TFRecordCompressionType',
    'tf.python_io.TFRecordOptions': 'tf.io.TFRecordOptions',
    'tf.python_io.TFRecordWriter': 'tf.io.TFRecordWriter',
    'tf.python_io.tf_record_iterator': 'tf.io.tf_record_iterator',
    'tf.qr': 'tf.linalg.qr',
    'tf.quantize': 'tf.quantization.quantize',
    'tf.quantize_v2': 'tf.compat.v1.quantize_v2',
    'tf.quantized_concat': 'tf.quantization.quantized_concat',
    'tf.random.get_seed': 'tf.compat.v1.random.get_seed',
    'tf.random.multinomial': 'tf.compat.v1.random.multinomial',
    'tf.random.stateless_multinomial': 'tf.compat.v1.random.stateless_multinomial',
    'tf.random_crop': 'tf.image.random_crop',
    'tf.random_gamma': 'tf.random.gamma',
    'tf.random_normal': 'tf.random.normal',
    'tf.random_poisson': 'tf.compat.v1.random_poisson',
    'tf.random_shuffle': 'tf.random.shuffle',
    'tf.random_uniform': 'tf.random.uniform',
    'tf.read_file': 'tf.io.read_file',
    'tf.real': 'tf.math.real',
    'tf.reciprocal': 'tf.math.reciprocal',
    'tf.reduce_join': 'tf.strings.reduce_join',
    'tf.regex_replace': 'tf.strings.regex_replace',
    'tf.report_uninitialized_variables': 'tf.compat.v1.report_uninitialized_variables',
    'tf.resource_loader.get_data_files_path': 'tf.compat.v1.resource_loader.get_data_files_path',
    'tf.resource_loader.get_path_to_datafile': 'tf.compat.v1.resource_loader.get_path_to_datafile',
    'tf.resource_loader.get_root_dir_with_all_resources': 'tf.compat.v1.resource_loader.get_root_dir_with_all_resources',
    'tf.resource_loader.load_resource': 'tf.compat.v1.resource_loader.load_resource',
    'tf.resource_loader.readahead_file_path': 'tf.compat.v1.resource_loader.readahead_file_path',
    'tf.reverse_v2': 'tf.reverse',
    'tf.rint': 'tf.math.rint',
    'tf.rsqrt': 'tf.math.rsqrt',
    'tf.saved_model.Builder': 'tf.compat.v1.saved_model.Builder',
    'tf.saved_model.LEGACY_INIT_OP_KEY': 'tf.compat.v1.saved_model.LEGACY_INIT_OP_KEY',
    'tf.saved_model.TRAINING': 'tf.saved_model.TRANING',
    'tf.saved_model.build_tensor_info': 'tf.compat.v1.saved_model.build_tensor_info',
    'tf.saved_model.builder.SavedModelBuilder': 'tf.compat.v1.saved_model.builder.SavedModelBuilder',
    'tf.saved_model.constants.ASSETS_DIRECTORY': 'tf.saved_model.ASSETS_DIRECTORY',
    'tf.saved_model.constants.ASSETS_KEY': 'tf.saved_model.ASSETS_KEY',
    'tf.saved_model.constants.LEGACY_INIT_OP_KEY': 'tf.compat.v1.saved_model.constants.LEGACY_INIT_OP_KEY',
    'tf.saved_model.constants.MAIN_OP_KEY': 'tf.saved_model.MAIN_OP_KEY',
    'tf.saved_model.constants.SAVED_MODEL_FILENAME_PB': 'tf.saved_model.SAVED_MODEL_FILENAME_PB',
    'tf.saved_model.constants.SAVED_MODEL_FILENAME_PBTXT': 'tf.saved_model.SAVED_MODEL_FILENAME_PBTXT',
    'tf.saved_model.constants.SAVED_MODEL_SCHEMA_VERSION': 'tf.saved_model.SAVED_MODEL_SCHEMA_VERSION',
    'tf.saved_model.constants.VARIABLES_DIRECTORY': 'tf.saved_model.VARIABLES_DIRECTORY',
    'tf.saved_model.constants.VARIABLES_FILENAME': 'tf.saved_model.VARIABLES_FILENAME',
    'tf.saved_model.experimental.save': 'tf.saved_model.save',
    'tf.saved_model.get_tensor_from_tensor_info': 'tf.compat.v1.saved_model.get_tensor_from_tensor_info',
    'tf.saved_model.load': 'tf.compat.v1.saved_model.load',
    'tf.saved_model.loader.load': 'tf.compat.v1.saved_model.loader.load',
    'tf.saved_model.loader.maybe_saved_model_directory': 'tf.compat.v1.saved_model.loader.maybe_saved_model_directory',
    'tf.saved_model.main_op.main_op': 'tf.compat.v1.saved_model.main_op.main_op',
    'tf.saved_model.main_op.main_op_with_restore': 'tf.compat.v1.saved_model.main_op.main_op_with_restore',
    'tf.saved_model.main_op_with_restore': 'tf.compat.v1.saved_model.main_op_with_restore',
    'tf.saved_model.maybe_saved_model_directory': 'tf.compat.v1.saved_model.maybe_saved_model_directory',
    'tf.saved_model.signature_constants.CLASSIFY_INPUTS': 'tf.saved_model.CLASSIFY_INPUTS',
    'tf.saved_model.signature_constants.CLASSIFY_METHOD_NAME': 'tf.saved_model.CLASSIFY_METHOD_NAME',
    'tf.saved_model.signature_constants.CLASSIFY_OUTPUT_CLASSES': 'tf.saved_model.CLASSIFY_OUTPUT_CLASSES',
    'tf.saved_model.signature_constants.CLASSIFY_OUTPUT_SCORES': 'tf.saved_model.CLASSIFY_OUTPUT_SCORES',
    'tf.saved_model.signature_constants.DEFAULT_SERVING_SIGNATURE_DEF_KEY': 'tf.saved_model.DEFAULT_SERVING_SIGNATURE_DEF_KEY',
    'tf.saved_model.signature_constants.PREDICT_INPUTS': 'tf.saved_model.PREDICT_INPUTS',
    'tf.saved_model.signature_constants.PREDICT_METHOD_NAME': 'tf.saved_model.PREDICT_METHOD_NAME',
    'tf.saved_model.signature_constants.PREDICT_OUTPUTS': 'tf.saved_model.PREDICT_OUTPUTS',
    'tf.saved_model.signature_constants.REGRESS_INPUTS': 'tf.saved_model.REGRESS_INPUTS',
    'tf.saved_model.signature_constants.REGRESS_METHOD_NAME': 'tf.saved_model.REGRESS_METHOD_NAME',
    'tf.saved_model.signature_constants.REGRESS_OUTPUTS': 'tf.saved_model.REGRESS_OUTPUTS',
    'tf.saved_model.signature_def_utils.build_signature_def': 'tf.saved_model.build_signature_def',
    'tf.saved_model.signature_def_utils.classification_signature_def': 'tf.saved_model.classification_signature_def',
    'tf.saved_model.signature_def_utils.is_valid_signature': 'tf.saved_model.is_valid_signature',
    'tf.saved_model.signature_def_utils.predict_signature_def': 'tf.saved_model.predict_signature_def',
    'tf.saved_model.signature_def_utils.regression_signature_def': 'tf.saved_model.regression_signature_def',
    'tf.saved_model.simple_save': 'tf.compat.v1.saved_model.simple_save',
    'tf.saved_model.tag_constants.GPU': 'tf.saved_model.GPU',
    'tf.saved_model.tag_constants.SERVING': 'tf.saved_model.SERVING',
    'tf.saved_model.tag_constants.TPU': 'tf.saved_model.TPU',
    'tf.saved_model.tag_constants.TRAINING': 'tf.saved_model.TRANING',
    'tf.saved_model.utils.build_tensor_info': 'tf.compat.v1.saved_model.utils.build_tensor_info',
    'tf.saved_model.utils.get_tensor_from_tensor_info': 'tf.compat.v1.saved_model.utils.get_tensor_from_tensor_info',
    'tf.scatter_add': 'tf.compat.v1.scatter_add',
    'tf.scatter_nd_add': 'tf.compat.v1.scatter_nd_add',
    'tf.scatter_nd_sub': 'tf.compat.v1.scatter_nd_sub',
    'tf.scatter_nd_update': 'tf.compat.v1.scatter_nd_update',
    'tf.scatter_sub': 'tf.compat.v1.scatter_sub',
    'tf.scatter_update': 'tf.compat.v1.scatter_update',
    'tf.segment_max': 'tf.math.segment_max',
    'tf.segment_mean': 'tf.math.segment_mean',
    'tf.segment_min': 'tf.math.segment_min',
    'tf.segment_prod': 'tf.math.segment_prod',
    'tf.segment_sum': 'tf.math.segment_sum',
    'tf.self_adjoint_eig': 'tf.linalg.eigh',
    'tf.self_adjoint_eigvals': 'tf.linalg.eigvalsh',
    'tf.serialize_many_sparse': 'tf.io.serialize_many_sparse',
    'tf.serialize_sparse': 'tf.io.serialize_sparse',
    'tf.serialize_tensor': 'tf.io.serialize_tensor',
    'tf.set_random_seed': 'tf.random.set_random_seed',
    'tf.setdiff1d': 'tf.compat.v1.setdiff1d',
    'tf.sets.set_difference': 'tf.sets.difference',
    'tf.sets.set_intersection': 'tf.sets.intersection',
    'tf.sets.set_size': 'tf.sets.size',
    'tf.sets.set_union': 'tf.sets.union',
    'tf.space_to_batch': 'tf.nn.space_to_batch',
    'tf.space_to_depth': 'tf.nn.space_to_depth',
    'tf.sparse.matmul': 'tf.sparse.sparse_dense_matmul',
    'tf.sparse.merge': 'tf.compat.v1.sparse.merge',
    'tf.sparse.placeholder': 'tf.compat.v1.sparse.placeholder',
    'tf.sparse.reduce_max_sparse': 'tf.compat.v1.sparse.reduce_max_sparse',
    'tf.sparse_add': 'tf.compat.v1.sparse_add',
    'tf.sparse_fill_empty_rows': 'tf.sparse.fill_empty_rows',
    'tf.sparse_mask': 'tf.sparse.mask',
    'tf.sparse_matmul': 'tf.compat.v1.sparse_matmul',
    'tf.sparse_maximum': 'tf.sparse.maximum',
    'tf.sparse_merge': 'tf.compat.v1.sparse_merge',
    'tf.sparse_minimum': 'tf.sparse.minimum',
    'tf.sparse_placeholder': 'tf.compat.v1.sparse_placeholder',
    'tf.sparse_reduce_max': 'tf.compat.v1.sparse_reduce_max',
    'tf.sparse_reduce_max_sparse': 'tf.compat.v1.sparse_reduce_max_sparse',
    'tf.sparse_reduce_sum': 'tf.sparse.reduce_sum',
    'tf.sparse_reduce_sum_sparse': 'tf.sparse.reduce_sum_sparse',
    'tf.sparse_reorder': 'tf.sparse.reorder',
    'tf.sparse_reset_shape': 'tf.sparse.reset_shape',
    'tf.sparse_reshape': 'tf.sparse.reshape',
    'tf.sparse_retain': 'tf.sparse.retain',
    'tf.sparse_segment_mean': 'tf.compat.v1.sparse_segment_mean',
    'tf.sparse_segment_sqrt_n': 'tf.compat.v1.sparse_segment_sqrt_n',
    'tf.sparse_segment_sum': 'tf.compat.v1.sparse_segment_sum',
    'tf.sparse_slice': 'tf.sparse.slice',
    'tf.sparse_softmax': 'tf.sparse.softmax',
    'tf.sparse_split': 'tf.compat.v1.sparse_split',
    'tf.sparse_tensor_dense_matmul': 'tf.sparse.sparse_dense_matmul',
    'tf.sparse_tensor_to_dense': 'tf.sparse.to_dense',
    'tf.sparse_to_indicator': 'tf.sparse.to_indicator',
    'tf.sparse_transpose': 'tf.sparse.transpose',
    'tf.spectral.dct': 'tf.signal.dct',
    'tf.spectral.fft': 'tf.signal.fft',
    'tf.spectral.fft2d': 'tf.signal.fft2d',
    'tf.spectral.fft3d': 'tf.signal.fft3d',
    'tf.spectral.idct': 'tf.signal.idct',
    'tf.spectral.ifft': 'tf.signal.ifft',
    'tf.spectral.ifft2d': 'tf.signal.ifft2d',
    'tf.spectral.ifft3d': 'tf.signal.ifft3d',
    'tf.spectral.irfft': 'tf.signal.irfft',
    'tf.spectral.irfft2d': 'tf.signal.irfft2d',
    'tf.spectral.irfft3d': 'tf.signal.irfft3d',
    'tf.spectral.rfft': 'tf.signal.rfft',
    'tf.spectral.rfft2d': 'tf.signal.rfft2d',
    'tf.spectral.rfft3d': 'tf.signal.rfft3d',
    'tf.squared_difference': 'tf.math.squared_difference',
    'tf.string_join': 'tf.strings.join',
    'tf.string_strip': 'tf.strings.strip',
    'tf.string_to_hash_bucket': 'tf.strings.to_hash_bucket',
    'tf.string_to_hash_bucket_fast': 'tf.strings.to_hash_bucket_fast',
    'tf.string_to_hash_bucket_strong': 'tf.strings.to_hash_bucket_strong',
    'tf.string_to_number': 'tf.strings.to_number',
    'tf.svd': 'tf.linalg.svd',
    'tf.tables_initializer': 'tf.compat.v1.tables_initializer',
    'tf.test.compute_gradient': 'tf.compat.v1.test.compute_gradient',
    'tf.test.compute_gradient_error': 'tf.compat.v1.test.compute_gradient_error',
    'tf.test.get_temp_dir': 'tf.compat.v1.test.get_temp_dir',
    'tf.test.mock': 'tf.compat.v1.test.mock',
    'tf.test.test_src_dir_path': 'tf.compat.v1.test.test_src_dir_path',
    'tf.to_bfloat16': 'tf.compat.v1.to_bfloat16',
    'tf.to_complex128': 'tf.compat.v1.to_complex128',
    'tf.to_complex64': 'tf.compat.v1.to_complex64',
    'tf.to_double': 'tf.compat.v1.to_double',
    'tf.to_float': 'tf.compat.v1.to_float',
    'tf.to_int32': 'tf.compat.v1.to_int32',
    'tf.to_int64': 'tf.compat.v1.to_int64',
    'tf.trace': 'tf.linalg.trace',
    'tf.train.MonitoredTrainingSession': 'tf.compat.v1.train.MonitoredTrainingSession',
    'tf.train.NewCheckpointReader': 'tf.compat.v1.train.NewCheckpointReader',
    'tf.train.ProfilerHook': 'tf.compat.v1.train.ProfilerHook',
    'tf.train.QueueRunner': 'tf.compat.v1.train.QueueRunner',
    'tf.train.Saver': 'tf.compat.v1.train.Saver',
    'tf.train.SaverDef': 'tf.compat.v1.train.SaverDef',
    'tf.train.SyncReplicasOptimizer': 'tf.compat.v1.train.SyncReplicasOptimizer',
    'tf.train.add_queue_runner': 'tf.compat.v1.train.add_queue_runner',
    'tf.train.assert_global_step': 'tf.compat.v1.train.assert_global_step',
    'tf.train.basic_train_loop': 'tf.compat.v1.train.basic_train_loop',
    'tf.train.batch': 'tf.compat.v1.train.batch',
    'tf.train.batch_join': 'tf.compat.v1.train.batch_join',
    'tf.train.checkpoint_exists': 'tf.compat.v1.train.checkpoint_exists',
    'tf.train.create_global_step': 'tf.compat.v1.train.create_global_step',
    'tf.train.do_quantize_training_on_graphdef': 'tf.compat.v1.train.do_quantize_training_on_graphdef',
    'tf.train.export_meta_graph': 'tf.compat.v1.train.export_meta_graph',
    'tf.train.generate_checkpoint_state_proto': 'tf.compat.v1.train.generate_checkpoint_state_proto',
    'tf.train.get_checkpoint_mtimes': 'tf.compat.v1.train.get_checkpoint_mtimes',
    'tf.train.get_global_step': 'tf.compat.v1.train.get_global_step',
    'tf.train.get_or_create_global_step': 'tf.compat.v1.train.get_or_create_global_step',
    'tf.train.global_step': 'tf.compat.v1.train.global_step',
    'tf.train.import_meta_graph': 'tf.compat.v1.train.import_meta_graph',
    'tf.train.init_from_checkpoint': 'tf.compat.v1.train.init_from_checkpoint',
    'tf.train.input_producer': 'tf.compat.v1.train.input_producer',
    'tf.train.limit_epochs': 'tf.compat.v1.train.limit_epochs',
    'tf.train.match_filenames_once': 'tf.io.match_filenames_once',
    'tf.train.maybe_batch': 'tf.compat.v1.train.maybe_batch',
    'tf.train.maybe_batch_join': 'tf.compat.v1.train.maybe_batch_join',
    'tf.train.maybe_shuffle_batch': 'tf.compat.v1.train.maybe_shuffle_batch',
    'tf.train.maybe_shuffle_batch_join': 'tf.compat.v1.train.maybe_shuffle_batch_join',
    'tf.train.queue_runner.QueueRunner': 'tf.compat.v1.train.queue_runner.QueueRunner',
    'tf.train.queue_runner.add_queue_runner': 'tf.compat.v1.train.queue_runner.add_queue_runner',
    'tf.train.queue_runner.start_queue_runners': 'tf.compat.v1.train.queue_runner.start_queue_runners',
    'tf.train.range_input_producer': 'tf.compat.v1.train.range_input_producer',
    'tf.train.remove_checkpoint': 'tf.compat.v1.train.remove_checkpoint',
    'tf.train.replica_device_setter': 'tf.compat.v1.train.replica_device_setter',
    'tf.train.shuffle_batch': 'tf.compat.v1.train.shuffle_batch',
    'tf.train.shuffle_batch_join': 'tf.compat.v1.train.shuffle_batch_join',
    'tf.train.slice_input_producer': 'tf.compat.v1.train.slice_input_producer',
    'tf.train.start_queue_runners': 'tf.compat.v1.train.start_queue_runners',
    'tf.train.string_input_producer': 'tf.compat.v1.train.string_input_producer',
    'tf.train.update_checkpoint_state': 'tf.compat.v1.train.update_checkpoint_state',
    'tf.train.write_graph': 'tf.io.write_graph',
    'tf.trainable_variables': 'tf.compat.v1.trainable_variables',
    'tf.uniform_unit_scaling_initializer': 'tf.initializers.uniform_unit_scaling',
    'tf.unsorted_segment_max': 'tf.math.unsorted_segment_max',
    'tf.unsorted_segment_mean': 'tf.math.unsorted_segment_mean',
    'tf.unsorted_segment_min': 'tf.math.unsorted_segment_min',
    'tf.unsorted_segment_prod': 'tf.math.unsorted_segment_prod',
    'tf.unsorted_segment_sqrt_n': 'tf.math.unsorted_segment_sqrt_n',
    'tf.unsorted_segment_sum': 'tf.math.unsorted_segment_sum',
    'tf.variable_op_scope': 'tf.compat.v1.variable_op_scope',
    'tf.variable_scope': 'tf.compat.v1.variable_scope',
    'tf.variables_initializer': 'tf.compat.v1.variables_initializer',
    'tf.variance_scaling_initializer': 'tf.keras.initializers.VarianceScaling',
    'tf.verify_tensor_all_finite': 'tf.compat.v1.verify_tensor_all_finite',
    'tf.wrap_function': 'tf.compat.v1.wrap_function',
    'tf.write_file': 'tf.io.write_file',
    'tf.zeta': 'tf.math.zeta'
}
