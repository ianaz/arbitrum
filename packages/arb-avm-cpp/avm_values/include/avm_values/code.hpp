/*
 * Copyright 2020, Offchain Labs, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef code_hpp
#define code_hpp

#include <avm_values/bigint.hpp>

#include <atomic>
#include <cassert>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

class LoadedCodeSegment;
struct CodePoint;
struct CodePointStub;
struct Operation;
struct Slot;

struct CodeSegmentInner {
    uint64_t segment_id;
    std::vector<CodePoint> code;
    std::shared_mutex mutex;

    CodeSegmentInner(const CodeSegmentInner&) = delete;
    CodeSegmentInner& operator=(const CodeSegmentInner&) = delete;

    CodeSegmentInner(CodeSegmentInner&&) = delete;
    CodeSegmentInner& operator=(CodeSegmentInner&&) = delete;

    CodeSegmentInner(uint64_t segment_id_);
    CodeSegmentInner(uint64_t segment_id_, std::vector<CodePoint> code_);
};

class CodeSegment {
    friend LoadedCodeSegment;
    friend CodeSegment deserializeCodeSegment(
        std::vector<unsigned char>::const_iterator& bytes,
        std::vector<Slot>& slots);
    friend CodePointStub deserializeCodePointStub(
        std::vector<unsigned char>::const_iterator& bytes,
        std::vector<Slot>& slots);

    static std::atomic<uint64_t> next_segment_id;

    std::shared_ptr<CodeSegmentInner> inner;

    CodeSegment(std::shared_ptr<CodeSegmentInner> inner_) : inner(inner_) {}

    // Mutex must be acquired before calling this
    CodeSegment cloneWithSize(uint64_t size) const;

    static CodeSegment restoreCodeSegment(uint64_t segment_id,
                                          std::vector<CodePoint> code);
    static CodeSegment uninitialized() {
        return CodeSegment(std::shared_ptr<CodeSegmentInner>());
    }

   public:
    static CodeSegment newSegment();

    uint64_t segmentID() const { return inner->segment_id; }

    bool operator==(const CodeSegment& other) const {
        return inner.get() == other.inner.get();
    }

    bool operator!=(const CodeSegment& other) const {
        return inner.get() != other.inner.get();
    }

    CodePointStub addOperationAt(Operation op, uint64_t pc);

    LoadedCodeSegment load() const;
};

uint256_t segmentIdToDbHash(uint64_t segment_id);

uint256_t hash(CodeSegment segment) {
    return segmentIdToDbHash(segment.segmentID());
}

class LoadedCodeSegment : public CodeSegment {
    std::shared_lock<std::shared_mutex> guard;

   public:
    LoadedCodeSegment(CodeSegment segment_);

    const CodePoint& operator[](uint64_t pc) const;
    std::vector<CodePoint>::const_iterator begin() const;
    std::vector<CodePoint>::const_iterator end() const;
    size_t size() const;
};

#endif /* code_hpp */
