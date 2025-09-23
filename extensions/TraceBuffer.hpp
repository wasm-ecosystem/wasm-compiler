///
/// @file TraceBuffer.hpp
/// @copyright Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
/// SPDX-License-Identifier: Apache-2.0
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <deque>
#include <memory>
#include <new>

#include "src/core/common/Span.hpp"

namespace vb {
namespace extension {

constexpr size_t DefaultBufferSize{65536U}; ///< Default size of the trace buffer
constexpr size_t MinSwapBufferSize{4096U};  ///< When the trace in buffer is more than this size, the whole buffer will be swapped out

/// @brief detail item of the trace point
struct TraceItems final {
  uint32_t timePoint; ///< Time point of the trace point
  uint32_t traceId;   ///< Trace ID of the trace point

  /// @brief constructor
  TraceItems(uint32_t ts, uint32_t id) : timePoint(ts), traceId(id) {
  }
};

struct alignas(32) BufferType : public std::array<uint32_t, DefaultBufferSize> {};

/// @brief buffer for trace
class TraceBuffer final {
  std::unique_ptr<BufferType> ptr_; ///< buffer

  /// @brief get current size of the trace
  size_t getSizeImpl() const noexcept {
    return ptr_->at(1U);
  }

public:
  /// @brief decision whether owned real buffer or not
  enum class InitState { Uninitialized, Initialized };
  /// @brief constructor
  explicit TraceBuffer(InitState state) : ptr_(nullptr) {
    if (state == InitState::Initialized) {
      ptr_.reset(new BufferType());
    }
  }
  /// @brief get span of the buffer
  Span<uint32_t> getSpan() noexcept {
    return Span<uint32_t>(ptr_->data(), ptr_->size());
  }
  /// @brief is the buffer nearly full
  bool isNearlyFull() const noexcept {
    assert(ptr_ != nullptr);
    return getSizeImpl() >= MinSwapBufferSize;
  }

  /// @brief get current size of the trace
  size_t getSize() const noexcept {
    return ptr_ == nullptr ? 0U : getSizeImpl();
  }
  /// @brief get trace item in index
  TraceItems getTraceItem(size_t index) const noexcept {
    assert(index < getSize());
    return TraceItems(ptr_->at(2U + 2U * index), ptr_->at(2U + 2U * index + 1U));
  }
};

/// @brief Class for record trace data
class TraceRecorder final {
public:
  TraceRecorder() = default;
  /// @brief push current buffer to completed record buffer deque
  Span<uint32_t> swapOut() {
    buffers_.emplace_back(std::move(lastBuffer_));
    lastBuffer_ = std::move(currentBuffer_);
    currentBuffer_ = TraceBuffer{TraceBuffer::InitState::Initialized};
    return currentBuffer_.getSpan();
  }

  bool needSwapOut() const noexcept {
    return currentBuffer_.isNearlyFull();
  }

  std::deque<TraceBuffer> moveOutBuffers() noexcept {
    std::deque<TraceBuffer> buffer{std::move(buffers_)};
    return buffer;
  }

  size_t getCurrentBufferSize() const noexcept {
    return currentBuffer_.getSize();
  }

private:
  TraceBuffer currentBuffer_{TraceBuffer::InitState::Uninitialized}; // immutable
  TraceBuffer lastBuffer_{TraceBuffer::InitState::Uninitialized};    // immutable
  std::deque<TraceBuffer> buffers_;                                  // mutable
};

} // namespace extension
} // namespace vb
