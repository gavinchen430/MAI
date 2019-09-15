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

#include <string.h>

#include "BufferImpl.h"
#include "util/MAIType.h"

namespace MAI {

SimpleBuffer::SimpleBuffer(Allocator* allocator) :
    mAllocator(allocator),
    mOffset(0),
    mBufferPtr(NULL),
    mIsConst(false),
    mSize(0) {
}

SimpleBuffer::~SimpleBuffer(){}

MAI_STATUS SimpleBuffer::allocate(int64 bytes) {
    MAI_CHECK(mAllocator != NULL, "Allocator is null");
    MAI_CHECK(mBufferPtr == NULL, "Buffer has been allocated");
    mBufferPtr = mAllocator->allocate(bytes);
    mSize = bytes;
    return MAI_SUCCESS;
}

void SimpleBuffer::setBufferAddr(const uint8* buffer, uint32 offset) {
    MAI_CHECK(mBufferPtr == NULL, "Buffer has been allocated");
    mIsConst = true;
    mBufferPtr = const_cast<uint8*>(buffer);
    mOffset = offset;
}

void SimpleBuffer::setBufferAddr(uint8* buffer, uint32 offset) {
    MAI_CHECK(mBufferPtr == NULL, "Buffer has been allocated");
    mIsConst = false;
    mBufferPtr = buffer;
    mOffset = offset;
}

void SimpleBuffer::copy(const uint8* src, int32 offset, int64 len) {
    MAI_CHECK(mBufferPtr != NULL, "Buffer is null");
    memcpy(mBufferPtr, src + offset, len);
}

const uint8* SimpleBuffer::data() {
    return mBufferPtr;
}

uint8* SimpleBuffer::mutableData() {
    MAI_CHECK(!mIsConst, "Buffer is not mutable");
    return mBufferPtr;
}

void SimpleBuffer::resize(uint64 len) {
    if (mSize != len) {
        delete mBufferPtr;
        mBufferPtr = NULL;
    }
    allocate(len);
}

uint64 SimpleBuffer::size() {
    return mSize;
}

} //namespace MAI
