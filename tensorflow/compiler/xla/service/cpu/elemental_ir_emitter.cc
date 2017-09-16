/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/compiler/xla/service/cpu/elemental_ir_emitter.h"

#include <string>

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "tensorflow/compiler/xla/service/hlo_opcode.h"
#include "tensorflow/compiler/xla/service/llvm_ir/llvm_util.h"
#include "tensorflow/compiler/xla/types.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/compiler/xla/xla_data.pb.h"

namespace xla {
namespace cpu {

StatusOr<llvm::Value*> CpuElementalIrEmitter::EmitFloatUnaryOp(
    const HloInstruction* op, llvm::Value* operand_value) const {
  switch (op->opcode()) {
    case HloOpcode::kTanh: {
      PrimitiveType element_type = op->shape().element_type();
      string function_name;
      switch (element_type) {
        case F32:
          function_name = "tanhf";
          break;
        case F64:
          function_name = "tanh";
          break;
        default:
          return Unimplemented("tanh");
      }
      // Create function type for the function.
      llvm::FunctionType* function_type = llvm::FunctionType::get(
          llvm_ir::PrimitiveTypeToIrType(element_type, ir_builder_),
          llvm_ir::PrimitiveTypeToIrType(element_type, ir_builder_),
          /*isVarArg=*/false);
      // Create function declaration for 'tanhf'.
      llvm::Function* function =
          llvm::cast<llvm::Function>(module_->getOrInsertFunction(
              llvm_ir::AsStringRef(function_name), function_type));
      function->setCallingConv(llvm::CallingConv::C);
      function->setDoesNotThrow();
      function->setDoesNotAccessMemory();
      // Create instruction to call 'tanhf'.
      return ir_builder_->CreateCall(function, operand_value);
    }
    default:
      return ElementalIrEmitter::EmitFloatUnaryOp(op, operand_value);
  }
}

llvm_ir::ElementGenerator CpuElementalIrEmitter::MakeElementGenerator(
    const HloInstruction* hlo,
    const HloToElementGeneratorMap& operand_to_generator) const {
  if (hlo->opcode() == HloOpcode::kMap) {
    return [this, hlo, &operand_to_generator](
               const llvm_ir::IrArray::Index& index) -> StatusOr<llvm::Value*> {
      std::vector<llvm::Value*> operands;
      for (int i = 0; i < hlo->operand_count(); i++) {
        TF_ASSIGN_OR_RETURN(llvm::Value * operand_value,
                            operand_to_generator.at(hlo->operand(i))(
                                ElementwiseSourceIndex(index, *hlo, 0)));
        operands.push_back(operand_value);
      }
      return ir_emitter_->EmitScalarCall(hlo->shape().element_type(),
                                         hlo->to_apply(), operands,
                                         llvm_ir::IrName(hlo));
    };
  }
  return ElementalIrEmitter::MakeElementGenerator(hlo, operand_to_generator);
}
}  // namespace cpu
}  // namespace xla
