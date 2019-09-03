// Copyright 2019 MAI. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

namespace MAI {
namespace Op {
namespace CPU {

#define DECLARE_REGISTER_OP(op) \
    extern void register##op();

#define REGISTER_OP(op) \
    register##op();

DECLARE_REGISTER_OP(Reshape);
DECLARE_REGISTER_OP(Shape);
DECLARE_REGISTER_OP(Squeeze);
DECLARE_REGISTER_OP(BiasAdd);
DECLARE_REGISTER_OP(Relu);
DECLARE_REGISTER_OP(Relu1);
DECLARE_REGISTER_OP(Relu6);
DECLARE_REGISTER_OP(Sigmoid);
DECLARE_REGISTER_OP(Softmax);
DECLARE_REGISTER_OP(FusedBatchNorm);

class CPURegister {
public:
    CPURegister() {
        REGISTER_OP(Reshape);
        REGISTER_OP(Shape);
        REGISTER_OP(Squeeze);
        REGISTER_OP(BiasAdd);
        REGISTER_OP(Relu);
        REGISTER_OP(Relu1);
        REGISTER_OP(Relu6);
        REGISTER_OP(Sigmoid);
        REGISTER_OP(Softmax);
        REGISTER_OP(FusedBatchNorm);
    }
    ~CPURegister() = default;
};

} // namespace CPU
} // namespace Op
} // namespace MAI
