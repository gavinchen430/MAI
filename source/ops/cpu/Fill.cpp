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

#include <cmath>
#include "core/OperatorRegister.h"
#include "util/MAIUtil.h"

namespace MAI {
namespace Op {
namespace CPU {

template<typename T>
class Fill : public Operator {
public:
    Fill() = default;
    ~Fill() = default;

    MAI_STATUS init() override {
        const Tensor* dims = getInputTensor(0);
        const Tensor* value = getInputTensor(1);
        Tensor* output = getOutputTensor(0);
        MAI_CHECK_NULL(dims);
        MAI_CHECK_NULL(output);
        MAI_CHECK(dims->dimSize() == 1, "dimSize of dims must be 1 but not %d", dims->dimSize());
        MAI_CHECK(value->isScalar(), "Tensor(%s) is not scalar", value->name().c_str());
        std::vector<shape_t> outputShape(dims->dim(0));
        const int32* dimsData = dims->data<int32>();
        for (shape_t i = 0; i < dims->elementSize(); ++i) {
            outputShape[i] = dimsData[i];
        }
        output->resize(outputShape);

        return MAI_SUCCESS;
    }

    MAI_STATUS run() override {
        const Tensor* dims = getInputTensor(0);
        const Tensor* value = getInputTensor(1);
        Tensor* output = getOutputTensor(0);

        const T* valueData = value->data<T>();
        T* outputData = output->mutableData<T>();
        std::fill(outputData, outputData + output->elementSize(), *valueData);
        return MAI_SUCCESS;
    }
};

void registerFill() {
    MAI_REGISTER_OP((OpContext{.opType=FILL,}), float, Fill);
    MAI_REGISTER_OP((OpContext{.opType=FILL,}), int32, Fill);
}

} // namespace CPU
} // namespace Op
} // namespace MAI
