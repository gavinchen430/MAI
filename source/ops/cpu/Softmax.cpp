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

#include "core/OperatorRegister.h"
#include "util/MAIUtil.h"

namespace MAI {
namespace Op {
namespace CPU {

template<typename T>
class Softmax : public Operator {
public:
    Softmax() = default;
    ~Softmax() = default;

    MAI_STATUS init() override {
        return MAI_SUCCESS;
    }

    MAI_STATUS run() override {
        const Tensor* input = getInputTensor(0);
        Tensor* output = getOutputTensor(0);
        MAI_CHECK_NULL(input);
        MAI_CHECK_NULL(output);
        output->resize({input->shape().size()});
        T* outputData = output->mutableData<T>();
        for (shape_t i = 0; i < input->shape().size(); ++i) {
        }
        return MAI_SUCCESS;
    }
};

void registerSoftmax() {
    MAI_REGISTER_OP((OpContext{.opType=SOFTMAX,}), float, Softmax);
}

} // namespace CPU
} // namespace Op
} // namespace MAI
