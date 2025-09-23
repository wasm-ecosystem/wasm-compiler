///
/// @file Runner.hpp
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
#ifndef EXTENSIONS_RUNNER_HPP
#define EXTENSIONS_RUNNER_HPP

#include <atomic>
#include <condition_variable>
#include <functional>
#include <initializer_list>
#include <thread>

namespace vb {
namespace extension {

/// @brief Thread wrapper that supports stop and resume functionality
class ThreadRunner {
public:
  /// @brief Thread state enumeration
  enum class State {
    Finished, ///< Thread is stopped
    Running,  ///< Thread is running
    Paused    ///< Thread is paused
  };

  /// @brief Constructor
  /// @param task The function to execute in the thread
  explicit ThreadRunner(std::function<void()> task);

  /// @brief Destructor - stops the thread if running
  ~ThreadRunner();

  // Delete copy constructor and assignment operator
  ThreadRunner(const ThreadRunner &) = delete;
  ThreadRunner &operator=(const ThreadRunner &) = delete;

  // Delete move constructor and assignment operator
  ThreadRunner(ThreadRunner &&) = delete;
  ThreadRunner &operator=(ThreadRunner &&) = delete;

  /// @brief Start the thread execution
  void start();

  /// @brief Resume the thread execution from paused state
  void resume();

  /// @brief Stop the thread execution
  void finish(std::initializer_list<std::condition_variable *> cvs);

  /// @brief Pause the thread execution
  void pause(std::initializer_list<std::condition_variable *> cvs);

  /// @brief is running state
  /// @return true if the thread is currently running
  bool isRunning() const {
    return currentState_.load() == State::Running && targetState_.load() == State::Running;
  }

private:
  /// @brief Internal thread function that handles the execution loop
  void threadFunction();

  std::function<void()> task_;      ///< User-provided task function
  std::thread thread_;              ///< Underlying thread object
  std::atomic<State> currentState_; ///< current thread state
  std::atomic<State> targetState_;  ///< target state
};

} // namespace extension
} // namespace vb

#endif // EXTENSIONS_RUNNER_HPP
