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

#include "util/MAIType.h"

namespace MAI {

class Allocator {
public:
    virtual ~Allocator() {}
    virtual uint8* allocate(uint64 bytes) = 0;
    virtual void deallocate(uint8* buffer, uint64 bytes) = 0;
};

class CPUAllocator : public Allocator {
public:
    uint8* allocate(uint64 bytes) {
        if (0 == bytes) {
            return NULL;
        }
        void* data = NULL;
        data = malloc(bytes);
        return (uint8*)data;
    }

    void deallocate(uint8* buffer, uint64 bytes) {
        MAI_UNUSED(bytes);
        free(buffer);
    }
};

} // namespace MAI
