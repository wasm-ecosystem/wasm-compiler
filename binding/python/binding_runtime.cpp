/*
 * Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <atomic>
#include <cstdint>
#include <cstring>
#include <future>
#include <iostream>
#include <memory>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <variant>
#include <vector>

#include "binding/python/binding.hpp"

#include "src/core/common/NativeSymbol.hpp"
#include "src/core/common/function_traits.hpp"
#include "src/core/common/util.hpp"
#include "src/core/runtime/Runtime.hpp"
#include "src/utils/ExecutableMemory.hpp"
#include "src/utils/SignalFunctionWrapper.hpp"

#if ACTIVE_STACK_OVERFLOW_CHECK
#include "src/utils/StackTop.hpp"
#endif
#if !LINEAR_MEMORY_BOUNDS_CHECKS
#include "src/utils/LinearMemoryAllocator.hpp"
#endif

namespace vb {
namespace binding {

namespace {
template <class T> class WasmValue {
  T value_{};

public:
  using Type = T;
  WasmValue() = default;
  explicit WasmValue(T value) : value_(value) {
  }
  T getValue() const {
    return value_;
  }
  std::string toString() {
    return std::to_string(value_);
  }
};
using WasmValueVariant = std::variant<WasmValue<int32_t>, WasmValue<int64_t>, WasmValue<float>, WasmValue<double>>;
} // namespace

class RuntimeWrapper {
public:
  static Span<uint8_t> getLinearMemoryArea(uint32_t const offset, uint32_t const size) {
    uint8_t *const ptr = self_->runtime_.getLinearMemoryRegion(offset, size);
    return {ptr, size};
  }

  static void info(uint32_t const offset, uint32_t const size, void *const ctx) {
    static_cast<void>(ctx);
    Span<uint8_t> const span = getLinearMemoryArea(offset, size);
    std::cout << std::string_view{vb::pCast<char *>(span.data()), span.size()} << std::endl;
  }

  void load(ManagedBinary const &binaryModule) {
    executableMemory_ = std::make_unique<ExecutableMemory>(ExecutableMemory::make_executable_copy(binaryModule));
    auto dynamicallyLinkedSymbols = make_array(NativeSymbol{NativeSymbol{
        NativeSymbol::Linkage::DYNAMIC,
        "log",
        "info",
        "(ii)",
        vb::pCast<const void *>(&info),
    }});

#if LINEAR_MEMORY_BOUNDS_CHECKS
    runtime_ = Runtime(*executableMemory_, memoryFnc, dynamicallyLinkedSymbols, nullptr);
#if ACTIVE_STACK_OVERFLOW_CHECK
    runtime_.setStackFence(getStackTop());
#endif // ACTIVE_STACK_OVERFLOW_CHECK
#else  // LINEAR_MEMORY_BOUNDS_CHECKS
    runtime_ = Runtime(*executableMemory_, allocator_, dynamicallyLinkedSymbols, nullptr);
#endif // LINEAR_MEMORY_BOUNDS_CHECKS
  }

  template <class Fn> void executeWasm(Fn const &fn) {
    enum class State { Init, Finished };
    std::atomic<State> state = State::Init;
    std::future<void> future = std::async(std::launch::async, [this, &fn, &state]() -> void {
      self_ = this;
      struct Raii { // NOLINT(cppcoreguidelines-special-member-functions)
        std::atomic<State> &state_;
        explicit Raii(std::atomic<State> &state) : state_(state) {
        }
        ~Raii() {
          state_ = State::Finished;
        }
      };
      Raii const raii{state};
      fn();
    });
    while (state != State::Finished) {
      if (PyErr_CheckSignals() != 0) {
#if INTERRUPTION_REQUEST
        runtime_.requestInterruption();
#endif
        std::cerr << "interrupt wasm execution by ctrl+c\n";
        throw pybind11::error_already_set();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    future.get();
  }

  void start() {
    executeWasm([this]() {
      vb::SignalFunctionWrapper::start(runtime_);
    });
  }

  static std::string createExpectedSignature(std::vector<WasmValueVariant> const &args) {
    std::string expectedSignature{'('};
    for (auto arg : args) {
      std::visit(
          [&expectedSignature](auto const &v) {
            using T = std::decay_t<decltype(v)>;
            expectedSignature.push_back(TypeToSignature<typename T::Type>::getSignatureChar());
          },
          arg);
    }
    expectedSignature.push_back(')');
    return expectedSignature;
  }

  void writeToLinearMemoryByWasmValue(WasmValue<int32_t> const value, pybind11::bytes const &data) const {
    writeToLinearMemory(static_cast<uint32_t>(value.getValue()), data);
  }
  void writeToLinearMemory(uint32_t const offset, pybind11::bytes const &data) const {
    auto const dataView = static_cast<std::string_view>(data);
    uint32_t const size = static_cast<uint32_t>(dataView.size());
    uint8_t *const ptr = runtime_.getLinearMemoryRegion(offset, size);
    std::memcpy(ptr, dataView.data(), size);
  }
  pybind11::bytes readFromLinearMemoryWasmValue(WasmValue<int32_t> const offset, uint32_t const size) const {
    return readFromLinearMemory(static_cast<uint32_t>(offset.getValue()), size);
  }
  pybind11::bytes readFromLinearMemory(uint32_t const offset, uint32_t const size) const {
    uint8_t const *const ptr = runtime_.getLinearMemoryRegion(offset, size);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return pybind11::bytes{reinterpret_cast<char const *>(ptr), size};
  }

  std::vector<WasmValueVariant> call(std::string const &funcName, std::string const &signature, std::vector<WasmValueVariant> const &args) {
    std::vector<uint64_t> serArgs{};
    serArgs.resize(args.size());
    for (size_t i = 0; i < args.size(); ++i) {
      std::visit(
          [i, &args, &serArgs, &signature](auto const &arg) {
            using T = std::decay_t<decltype(arg)>;
            typename T::Type value = arg.getValue();
            std::memcpy(&serArgs[i], &value, sizeof(value));
            if (signature[i + 1] != TypeToSignature<typename T::Type>::getSignatureChar()) {
              throw std::runtime_error("invalid signature, expected " + createExpectedSignature(args));
            }
          },
          args[i]);
    }
    if (args.size() + 1U >= signature.size()) {
      throw std::runtime_error("invalid signature, expected " + createExpectedSignature(args));
    }
    if (signature[args.size() + 1U] != ')') {
      throw std::runtime_error("invalid signature, expected " + createExpectedSignature(args));
    }
    RawModuleFunction func = runtime_.getRawExportedFunctionByName(Span<char const>(funcName.c_str(), funcName.size()),
                                                                   Span<char const>(signature.c_str(), signature.size()));
    size_t const numResults = signature.size() - args.size() - 2;
    std::vector<uint8_t> results(numResults * 8);
    std::vector<WasmValueVariant> ret{};

    executeWasm([&func, &serArgs, &results, &ret, signature]() {
      vb::SignalFunctionWrapper::call(func, vb::pCast<uint8_t const *>(serArgs.data()), vb::pCast<uint8_t *>(results.data()));
      uint8_t const *resultPtr = results.data();
      std::string const resultsType = signature.substr(signature.find(")") + 1);
      for (char const type : resultsType) {
        switch (type) {
        case 'i': {
          int32_t actualValue{0U};
          std::memcpy(&actualValue, resultPtr, sizeof(actualValue));
          ret.push_back(WasmValue<int32_t>{actualValue});
          break;
        }
        case 'I': {
          int64_t actualValue{0U};
          std::memcpy(&actualValue, resultPtr, sizeof(actualValue));
          ret.push_back(WasmValue<int64_t>{actualValue});
          break;
        }
        case 'f': {
          float actualValue{0U};
          std::memcpy(&actualValue, resultPtr, sizeof(actualValue));
          ret.push_back(WasmValue<float>{actualValue});
          break;
        }
        case 'F': {
          double actualValue{0U};
          std::memcpy(&actualValue, resultPtr, sizeof(actualValue));
          ret.push_back(WasmValue<double>{actualValue});
          break;
        }
        default:
          throw std::runtime_error("invalid return type in signature");
        }
        resultPtr += 8;
      }
    });
    return ret;
  }

private:
#if !LINEAR_MEMORY_BOUNDS_CHECKS
  LinearMemoryAllocator allocator_;
#endif
  Runtime runtime_;
  std::unique_ptr<ExecutableMemory> executableMemory_;
  thread_local static RuntimeWrapper *self_;
};
thread_local RuntimeWrapper *RuntimeWrapper::self_ = nullptr;

} // namespace binding

void binding::bindingRuntime(pybind11::module_ &m) {
  namespace py = pybind11;

  py::class_<WasmValue<int32_t>>(m, "i32", pybind11::module_local())
      .def(py::init<int32_t>())
      .def("getValue", &WasmValue<int32_t>::getValue)
      .def("__repr__", &WasmValue<int32_t>::toString);
  py::class_<WasmValue<int64_t>>(m, "i64", pybind11::module_local())
      .def(py::init<int64_t>())
      .def("getValue", &WasmValue<int64_t>::getValue)
      .def("__repr__", &WasmValue<int64_t>::toString);
  py::class_<WasmValue<float>>(m, "f32", pybind11::module_local())
      .def(py::init<float>())
      .def("getValue", &WasmValue<float>::getValue)
      .def("__repr__", &WasmValue<float>::toString);
  py::class_<WasmValue<double>>(m, "f64", pybind11::module_local())
      .def(py::init<double>())
      .def("getValue", &WasmValue<double>::getValue)
      .def("__repr__", &WasmValue<double>::toString);

  py::class_<RuntimeWrapper>(m, "Runtime", pybind11::module_local())
      .def(py::init<>())
      .def("load", &RuntimeWrapper::load)
      .def("start", &RuntimeWrapper::start)
      .def("call", &RuntimeWrapper::call)
      .def("write_to_linear_memory", &RuntimeWrapper::writeToLinearMemoryByWasmValue)
      .def("write_to_linear_memory", &RuntimeWrapper::writeToLinearMemory)
      .def("read_from_linear_memory", &RuntimeWrapper::readFromLinearMemoryWasmValue)
      .def("read_from_linear_memory", &RuntimeWrapper::readFromLinearMemory);
}

} // namespace vb
