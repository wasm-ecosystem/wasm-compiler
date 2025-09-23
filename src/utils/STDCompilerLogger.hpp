///
/// @file STDCompilerLogger.hpp
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
#ifndef STD_COMPILER_LOGGER
#define STD_COMPILER_LOGGER

#include <iostream>

#include "src/core/common/ILogger.hpp"

namespace vb {
///
/// @brief Log compiler error message to std::cout
///
class STDCompilerLogger : public ILogger {
public:
  ///
  /// @brief Log const char*
  ///
  /// @param message
  /// @return const ILogger&
  ///
  inline ILogger &operator<<(char const *const message) override {
    std::cout << message;
    return *this;
  }
  ///
  /// @brief Log message in Span format
  ///
  /// @param message
  /// @return const ILogger&
  ///
  inline ILogger &operator<<(Span<char const> const &message) override {
    static_cast<void>(std::cout.write(message.data(), static_cast<std::streamsize>(message.size())));
    return *this;
  }
  ///
  /// @brief log error code
  ///
  /// @param errorCode
  /// @return const ILogger&
  ///
  inline ILogger &operator<<(uint32_t const errorCode) override {
    std::cout << errorCode;
    return *this;
  }

  /// @brief The type of function which can be executed by ILogger
  using ILoggerFunc = ILogger &(*)(ILogger &logger);
  ///
  /// @brief Allows usage of vb::endStatement
  ///
  /// @param fnc Function to be executed with the corresponding ILogger
  /// @return ILogger&
  inline ILogger &operator<<(ILoggerFunc const fnc) override {
    return ILogger::operator<<(fnc);
  }

  ///
  /// @brief Mark this statement as finished
  ///
  /// @param level Log level
  ///
  inline void endStatement(LogLevel const level) override {
    static_cast<void>(level);
    std::cout << std::endl;
  }
};

} // namespace vb

#endif
