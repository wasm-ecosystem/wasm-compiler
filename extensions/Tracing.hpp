///
/// @file Tracing.hpp
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
#ifndef EXTENSIONS_TRACING_HPP
#define EXTENSIONS_TRACING_HPP

#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>

#include "Runner.hpp"
#include "TraceBuffer.hpp"

#include "src/core/runtime/Runtime.hpp"

namespace vb {
namespace extension {

///
/// @brief trace extension
///
/// It owns 2 threads, record thread and write thread.
/// The record thread is responsible for collecting trace data from runtimes and putting it into @b recordedTraces_.
/// The write thread is responsible for writing @b recordedTraces_ to the trace file.
///
/// All publish methods are thread-safe.
///
class TracingExtension final {
  /// @brief record group with identifier, it is used to wrapper trace group and identifier
  class TraceGroupWithIdentifier {
  public:
    /// @brief constructor
    TraceGroupWithIdentifier(Runtime const &runtime, std::deque<TraceBuffer> &&traceGroup) noexcept
        : identifier_(&runtime), traceGroup_(std::move(traceGroup)) {
    }
    /// @brief see @b identifier_
    uint64_t getIdentifier() const noexcept {
      return static_cast<uint64_t>(pToNum(identifier_));
    }
    /// @brief see @b traceGroup_
    std::deque<TraceBuffer> const &getTraceGroups() const noexcept {
      return traceGroup_;
    }

  private:
    void const *identifier_;             ///< Identifier of the trace group, we use runtime ptr as identifier
    std::deque<TraceBuffer> traceGroup_; ///< trace in the group
  };

public:
  ~TracingExtension() noexcept; ///< Destructor
  TracingExtension() noexcept;  ///< Constructor
  /// @brief Constructor
  /// @param tracingFile file to write trace data
  /// @param maxItems maximum number of items to write to the file
  TracingExtension(std::unique_ptr<std::ostream> traceStream, uint32_t const maxItems) noexcept;
  TracingExtension(TracingExtension const &) = delete;            ///< Constructor
  TracingExtension(TracingExtension &&) = delete;                 ///< Constructor
  TracingExtension &operator=(TracingExtension const &) = delete; ///< Assignment operator
  TracingExtension &operator=(TracingExtension &&) = delete;      ///< Assignment operator

  /// @brief register runtime
  /// @note it will stop recoding temporarily and restart recording.
  void registerRuntime(Runtime &runtime);
  /// @brief unregister runtime
  /// @note it will stop recoding temporarily and restart recording.
  void unregisterRuntime(Runtime &runtime);

  /// @brief stop recording and write all data to the file
  /// @note This function will block until all data is written to the file
  void stopAndWriteData();

private:
  std::mutex globalOperationsMutex_; ///< Mutex for all operations

  bool isEnabled_;                            ///< Is tracing enabled
  std::unique_ptr<std::ostream> traceStream_; ///< Stream to write trace data
  size_t leftItems_;                          ///< Number of items left to write

  ThreadRunner recordRunner_; ///< Thread for recording trace data
  ThreadRunner writeRunner_;  ///< Thread for writing trace data

  std::map<Runtime *, TraceRecorder> registeredRuntimes_; ///< Registered runtimes with their trace records

  std::deque<TraceGroupWithIdentifier> recordedTraces_; ///< Recorded traces to write
  std::mutex recordedTracesMutex_;                      ///< Mutex for recorded traces
  std::condition_variable recordedTracesCV_;            ///< Condition variable for recorded traces

  /// @brief record the trace data
  /// @note running in record thread
  /// @param forceSwapOut force to swap out the buffer
  void recordOnce(bool const forceSwapOut);
  /// @brief put the record data to recorded records
  void putRecordDataToRecordedRecords();
  /// @brief force record current all traces
  void forceRecordAllTraces();

  /// @brief record the trace data
  /// @note running in write thread
  void writeOnce();
};

extern TracingExtension traceExtension;

} // namespace extension
} // namespace vb

#endif // EXTENSIONS_TRACING_HPP
