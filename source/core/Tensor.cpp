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

#include "Tensor.h"

#include "core/BufferImpl.h"
#include "core/Allocator.h"
#include "util/MAIUtil.h"

namespace MAI {

Tensor::Tensor(DataType dataType) :
    mName(""),
    mDataType(dataType),
    mDataFormat(NHWC),
    mBuffer(NULL),
    mAllocator(NULL),
    mFlag(MEMORY_OWNER | ALLOCATOR_OWNER) {
}

Tensor::Tensor(DataType dataType, Allocator* allocator) :
    mName(""),
    mDataType(dataType),
    mDataFormat(NHWC),
    mBuffer(NULL),
    mAllocator(allocator),
    mFlag(0) {
}

Tensor::~Tensor() {
    release();
}

void Tensor::release() {
    if ((mFlag & MEMORY_OWNER) && mBuffer) {
        delete mBuffer;
        mBuffer = NULL;
    }

    if ((mFlag & ALLOCATOR_OWNER) && mAllocator) {
        delete mAllocator;
        mAllocator = NULL;
    }
}

void Tensor::copy(const void* data, int64 len) {
    MAI_CHECK(mBuffer != NULL, "Buffer is null");
    mBuffer->copy((const uint8*)data, 0, len);
}

void Tensor::allocateBuffer(const std::vector<uint64>& shape) {
    MAI_CHECK(!shape.empty(), "Shape cannot be null");
    MAI_CHECK(mBuffer == NULL && mShape.empty(), "Buffer is not null or shape is not null");
    MAI_CHECK(mAllocator != NULL, "Allocator cannot be null");
    mShape = shape;
    mFlag |= MEMORY_OWNER;
    mBuffer = new SimpleBuffer(mAllocator);
    mBuffer->allocate(size());
}

void Tensor::resize(const std::vector<uint64>& shape) {
    if (mBuffer != NULL) {
        mShape = shape;
        if (size() > mBuffer->size()) {
            mBuffer->resize(size());
        }
    } else {
        allocateBuffer(shape);
    }
}

std::vector<uint64> Tensor::shape() const {
    return mShape;
}

uint64 Tensor::dim(uint8 i) const {
    return mShape[i];
}

uint8 Tensor::dimSize() const {
    return static_cast<uint8>(mShape.size());
}

uint64 Tensor::elementSize() const {
    return shapeToSize(mShape);
}

uint64 Tensor::size() const {
    uint8 dataSize = getDataTypeSize(mDataType);
    return shapeToSize(mShape) * dataSize;
}

void Tensor::reuse(const Tensor* tensor) {
    mFlag &= ~MEMORY_OWNER;
    mDataType = tensor->mDataType;
    mBuffer = tensor->mBuffer;
    mShape = tensor->mShape;
    mAllocator = tensor->mAllocator;
}

void Tensor::reshape(const std::vector<shape_t>& shape) {
    //TODO (gavinchen) check accumulate size of shape and mShape is equal
    mShape = shape;
}

} // namespace MAI
