///
/// @file Tracing.cpp
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
#ifndef EXTENSIONS_TRACING_CPP
#define EXTENSIONS_TRACING_CPP

#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <ios>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>
#include <sys/types.h>
#include <utility>

#include "extensions/Tracing.hpp"

#include "src/core/runtime/Runtime.hpp"

static constexpr const char *WARP_TRACING_RECORDER_FILE_ENV = "WARP_TRACING_RECORDER_FILE";
static constexpr const char *WARP_TRACING_RECORDER_MAX_ITEMS_ENV = "WARP_TRACING_RECORDER_MAX_ITEMS";

namespace vb {
namespace extension {

static std::unique_ptr<std::ostream> getTracingFile() {
  const char *const fileName = std::getenv(WARP_TRACING_RECORDER_FILE_ENV);
  if (fileName == nullptr) {
    return nullptr;
  }
  auto file{std::make_unique<std::ofstream>(fileName, std::ios::binary | std::ios::out)};
  if (!file->good()) {
    std::cerr << "Failed to open tracing file: " << fileName;
    std::exit(255);
  }
  return file;
}
static uint32_t getMaxItems() {
  const char *const maxItems = std::getenv(WARP_TRACING_RECORDER_MAX_ITEMS_ENV);
  if (maxItems == nullptr) {
    return static_cast<uint32_t>(-1);
  }
  try {
    return static_cast<uint32_t>(std::strtoull(maxItems, nullptr, 10));
  } catch (...) {
    std::cerr << "Invalid max items value: " << maxItems;
    std::exit(255);
  }
}

TracingExtension::~TracingExtension() noexcept {
  recordRunner_.finish({});
  writeRunner_.finish({&recordedTracesCV_});
}

TracingExtension::TracingExtension() noexcept : TracingExtension(getTracingFile(), getMaxItems()) {
}
TracingExtension::TracingExtension(std::unique_ptr<std::ostream> traceStream, uint32_t const maxItems) noexcept
    : recordRunner_([this]() {
        recordOnce(false);
      }),
      writeRunner_([this]() {
        writeOnce();
      }) {
  if (!traceStream) {
    isEnabled_ = false;
    return;
  }
  traceStream_ = std::move(traceStream);
  leftItems_ = maxItems;
  isEnabled_ = true;

  std::string const magic = "___WARP_TRACE___";
  traceStream_->write(magic.data(), static_cast<std::streamsize>(magic.size()));

  // force to init the record thread
  recordOnce(true);
  recordRunner_.start();
  writeRunner_.start();
}

void TracingExtension::putRecordDataToRecordedRecords() {
  // This function is called in record thread, we should make sure it will not waste to many time.
  // Using try lock can avoid spend too much time in mutex.
  // Since we will call its parent function in loop, it is essentially a spin lock.
  std::unique_lock<std::mutex> lock(recordedTracesMutex_, std::try_to_lock);
  if (lock.owns_lock()) {
    bool notify = false;
    for (std::pair<Runtime *const, TraceRecorder> &it : registeredRuntimes_) {
      std::deque<TraceBuffer> buffers = it.second.moveOutBuffers();
      if (buffers.empty()) {
        continue;
      }
      notify = true;
      recordedTraces_.emplace_back(*it.first, std::move(buffers));
    }
    lock.unlock();
    if (notify) {
      recordedTracesCV_.notify_one();
    }
  }
}
void TracingExtension::recordOnce(bool const forceSwapOut) {
  bool isSwapOut{false};
  for (std::pair<Runtime *const, TraceRecorder> &it : registeredRuntimes_) {
    TraceRecorder &record = it.second;
    if (forceSwapOut || record.needSwapOut()) {
      // set a new clean buffer for the runtime
      it.first->setTraceBuffer(record.swapOut());
      isSwapOut = true;
    }
  }
  if (!isSwapOut) {
    // this loop we do not need to swap out any buffer
    // then some heavy work can be done
    putRecordDataToRecordedRecords();
  }
}

void TracingExtension::writeOnce() {
  std::deque<TraceGroupWithIdentifier> collectedTraces{};
  {
    std::unique_lock<std::mutex> lock{recordedTracesMutex_};
    recordedTracesCV_.wait(lock, [this]() -> bool {
      return !recordedTraces_.empty() || !writeRunner_.isRunning();
    });
    collectedTraces = std::move(recordedTraces_);
  }
  for (TraceGroupWithIdentifier const &recordGroup : collectedTraces) {
    for (TraceBuffer const &records : recordGroup.getTraceGroups()) {
      uint64_t const id = recordGroup.getIdentifier();
      for (size_t i = 0U; i < records.getSize(); i++) {
        TraceItems const item = records.getTraceItem(i);
        if (leftItems_ == 0) {
          break;
        }
        leftItems_--;
        // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
        traceStream_->write(reinterpret_cast<char const *>(&id), sizeof(id));
        traceStream_->write(reinterpret_cast<char const *>(&item.timePoint), sizeof(item.timePoint));
        traceStream_->write(reinterpret_cast<char const *>(&item.traceId), sizeof(item.traceId));
        // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
      }
    }
  }
}

void TracingExtension::registerRuntime(Runtime &runtime) {
  if (!isEnabled_) {
    return;
  }
  std::lock_guard<std::mutex> const operationLock{globalOperationsMutex_};
  // To avoid race condition, we stop recording before modifying the registered runtimes.
  recordRunner_.pause({});
  registeredRuntimes_.insert(std::make_pair(&runtime, TraceRecorder{}));
  // force to init the record thread
  recordOnce(true);
  recordRunner_.resume();
}

void TracingExtension::unregisterRuntime(Runtime &runtime) {
  if (!isEnabled_) {
    return;
  }
  std::lock_guard<std::mutex> const operationLock{globalOperationsMutex_};
  recordRunner_.pause({});
  writeRunner_.pause({&recordedTracesCV_});
  forceRecordAllTraces();
  registeredRuntimes_.erase(&runtime);
  recordRunner_.resume();
  writeRunner_.resume();
}

void TracingExtension::stopAndWriteData() {
  if (!isEnabled_) {
    return;
  }
  std::lock_guard<std::mutex> const operationLock{globalOperationsMutex_};
  recordRunner_.finish({});
  writeRunner_.finish({&recordedTracesCV_});
  forceRecordAllTraces();
  writeOnce();
}

void TracingExtension::forceRecordAllTraces() {
  // manually record the last items
  recordOnce(true); // current -> last
  recordOnce(true); // last -> buffer
  // writer thread is stopped, so try lock collectedRecordsMutex_ must success.
  putRecordDataToRecordedRecords();
}

TracingExtension traceExtension{};

} // namespace extension
} // namespace vb

#endif // EXTENSIONS_TRACING_CPP
