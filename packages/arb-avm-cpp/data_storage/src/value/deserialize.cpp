/*
 * Copyright 2021, Offchain Labs, Inc.
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

#include <data_storage/value/deserialize.hpp>

#include <avm_values/code.hpp>
#include <avm_values/codepointstub.hpp>
#include <avm_values/tuple.hpp>

uint256_t deserializeNum(std::vector<unsigned char>::const_iterator& bytes,
                         SlotMap&) {
    return deserializeUint256t(bytes);
}
CodePointStub deserializeCodePointStub(
    std::vector<unsigned char>::const_iterator& bytes,
    SlotMap& slots) {
    auto segment_id = deserializeUint64t(bytes);
    auto pc = deserializeUint64t(bytes);
    auto next_hash = deserializeUint256t(bytes);
    CodeSegment segment = slots.codeSegmentSlot(segmentIdToDbHash(segment_id));
    return CodePointStub({segment, pc}, next_hash);
}
Tuple deserializeTuple(std::vector<unsigned char>::const_iterator& bytes,
                       SlotMap& slots,
                       size_t size) {
    Tuple tup = Tuple::createSizedTuple(size);
    for (uint64_t i = 0; i < size; i++) {
        auto ty = *bytes;
        value inner;
        if (ty == HASH_PRE_IMAGE) {
            bytes++;
            auto hash = deserializeUint256t(bytes);
            inner = slots.tupleSlot(hash);
        } else {
            inner = deserializeValue(bytes, slots);
        }
        tup.unsafe_set_element(i, std::move(inner));
    }
    return tup;
}
Buffer deserializeBuffer(std::vector<unsigned char>::const_iterator& bytes,
                         SlotMap& slots) {
    uint8_t depth = *bytes++;
    if (depth == 0) {
        Buffer::LeafData leaf;
        std::copy(bytes, bytes + 32, leaf.begin());
        bytes += 32;
        return Buffer(leaf);
    } else {
        auto left = std::make_shared<Buffer>();
        auto right = std::make_shared<Buffer>();
        for (uint8_t i = 0; i < 2; i++) {
            if (*bytes++ != HASH_PRE_IMAGE) {
                throw std::runtime_error(
                    "TODO: implement inline buffer deserialization");
            }
            auto hash = deserializeUint256t(bytes);
            auto ptr = i ? &right : &left;
            *ptr = slots.bufferSlot(hash);
        }
        auto ret = Buffer(left, right);
        ret.depth = depth;
        return ret;
    }
}
CodeSegment deserializeCodeSegment(
    std::vector<unsigned char>::const_iterator& bytes,
    SlotMap& slots) {
    auto segment_id = deserializeUint64t(bytes);
    auto num_code_points = deserializeUint64t(bytes);
    std::vector<CodePoint> code;
    code.reserve(num_code_points);
    for (uint64_t i = 0; i < num_code_points; i++) {
        bool has_immediate = *bytes++;
        auto op = static_cast<OpCode>(*bytes++);
        auto next_hash = deserializeUint256t(bytes);
        std::optional<value> immediate;
        if (has_immediate) {
            immediate = deserializeValue(bytes, slots);
        }
        code.emplace_back(Operation(op, immediate), next_hash);
    }
    return CodeSegment::restoreCodeSegment(segment_id, code);
}

value deserializeValue(std::vector<unsigned char>::const_iterator& bytes,
                       SlotMap& slots) {
    auto ty = *bytes++;
    switch (ty) {
        case BUFFER: {
            return deserializeBuffer(bytes, slots);
        }
        case NUM: {
            return deserializeNum(bytes, slots);
        }
        case CODE_POINT_STUB: {
            return deserializeCodePointStub(bytes, slots);
        }
        case HASH_PRE_IMAGE: {
            throw std::runtime_error("attempted to deserialize HASH_PRE_IMAGE");
        }
        case CODE_SEGMENT: {
            return deserializeCodeSegment(bytes, slots);
        }
        default: {
            auto size = ty - TUPLE;
            if (size > 8) {
                throw std::runtime_error(
                    "attempted to deserialize value with invalid typecode");
            }
            return deserializeTuple(bytes, slots, size);
        }
    }
}

void Slot::fillInner(Tuple inner, value val) {
    *inner.tpl = *std::get<Tuple>(val).tpl;
}
void Slot::fillInner(std::shared_ptr<Buffer> inner, value val) {
    *inner = std::get<Buffer>(val);
}
void Slot::fillInner(CodeSegment inner, value val) {
    inner.fillUninitialized(std::get<CodeSegment>(val));
}

void Slot::fill(value val) {
    std::visit([&](const auto& x) { Slot::fillInner(x, std::move(val)); },
               inner);
}

Tuple SlotMap::tupleSlot(uint256_t hash) {
    auto it = slots.find(hash);
    if (it != slots.end()) {
        return std::get<Tuple>(it->second.inner);
    }
    auto ret = Tuple::uninitialized();
    slots.insert_or_assign(hash, Slot(ret));
    return ret;
}

std::shared_ptr<Buffer> SlotMap::bufferSlot(uint256_t hash) {
    auto it = slots.find(hash);
    if (it != slots.end()) {
        return std::get<std::shared_ptr<Buffer>>(it->second.inner);
    }
    auto ret = std::make_shared<Buffer>();
    slots.insert_or_assign(hash, Slot(ret));
    return ret;
}

CodeSegment SlotMap::codeSegmentSlot(uint256_t hash) {
    auto it = slots.find(hash);
    if (it != slots.end()) {
        return std::get<CodeSegment>(it->second.inner);
    }
    auto ret = CodeSegment::uninitialized();
    slots.insert_or_assign(hash, Slot(ret));
    return ret;
}

bool SlotMap::empty() {
    return slots.empty();
}

std::pair<uint256_t, Slot> SlotMap::takeSlot() {
    auto it = slots.begin();
    assert(it != slots.end());
    std::pair<uint256_t, Slot> item = *it;
    slots.erase(it);
    return item;
}
