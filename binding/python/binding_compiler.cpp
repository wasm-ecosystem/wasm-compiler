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

#include <cstdint>
#include <memory>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "binding/python/binding.hpp"
#include "disassembler/color.hpp"
#include "disassembler/disassembler.hpp"
#include "extensions/Analytics.hpp"
#include "extensions/DwarfImpl.hpp"

#include "src/core/common/NativeSymbol.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/Compiler.hpp"
#include "src/utils/STDCompilerLogger.hpp"

namespace vb {
namespace binding {

class CompilerWrapper {
  struct DynNativeSymbolStorage {
    std::string moduleName;
    std::string symbolName;
    std::string signature;
  };

public:
  CompilerWrapper() : compiler_{memoryFnc, allocFnc, freeFnc, nullptr, memoryFnc, true} {
  }

  void setLogger(bool isEnabled) {
    if (isEnabled) {
      logger_ = std::make_unique<STDCompilerLogger>();
    } else {
      logger_.reset();
    }
    compiler_.setLogger(logger_.get());
  }

#if ENABLE_EXTENSIONS
  void setAnalytics(bool isEnabled) {
    if (isEnabled) {
      if (analytics_ == nullptr) {
        analytics_ = std::make_unique<extension::Analytics>();
      }
      compiler_.setAnalytics(analytics_.get());
    } else {
      analytics_.reset();
      compiler_.setAnalytics(nullptr);
    }
  }
  uint32_t getJitSize() const {
    if (analytics_ == nullptr) {
      throw std::runtime_error("Analytics is not enabled");
    }
    return analytics_->getJitSize();
  }
  uint32_t getSpillsToStackCount() const {
    if (analytics_ == nullptr) {
      throw std::runtime_error("Analytics is not enabled");
    }
    return analytics_->getSpillsToStackCount();
  }
  uint32_t getSpillsToRegCount() const {
    if (analytics_ == nullptr) {
      throw std::runtime_error("Analytics is not enabled");
    }
    return analytics_->getSpillsToRegCount();
  }

  void setDwarf5Generator(bool isenabled) {
    if (isenabled) {
      if (dwarfGenerator_ == nullptr) {
        dwarfGenerator_ = std::make_unique<extension::Dwarf5Generator>();
      }
      compiler_.setDwarfGenerator(dwarfGenerator_.get());
    } else {
      dwarfGenerator_.reset();
      compiler_.setDwarfGenerator(nullptr);
    }
  }
  std::string dumpDwarf() const {
    if (dwarfGenerator_ == nullptr) {
      throw std::runtime_error("DWARF5 does not enabled");
    }
    std::stringstream ss{};
    dwarfGenerator_->dump(ss);
    return ss.str();
  }
  pybind11::bytes getDwarfObject() const {
    if (dwarfGenerator_ == nullptr) {
      throw std::runtime_error("DWARF5 does not enabled");
    }
    std::vector<uint8_t> const dwarfData = dwarfGenerator_->toDwarfObject();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return pybind11::bytes{reinterpret_cast<const char *>(dwarfData.data()), dwarfData.size()};
  }
#endif

  void registerApi(std::string const &moduleName, std::string const &symbolName, std::string const &signature) {
    nativeSymbolStorage_.emplace_back(DynNativeSymbolStorage{
        moduleName,
        symbolName,
        signature,
    });
  }

  void setDebugMode(bool const isEnable) {
#if defined(JIT_TARGET_TRICORE)
    if (isEnable) {
      throw std::runtime_error("tricore backend does not support debug mode");
    }
#else
    if (isEnable) {
      compiler_.enableDebugMode(memoryFnc);
    } else {
      compiler_.disableDebugMode();
    }
#endif
  }

  void setStacktraceRecordCount(uint32_t const count) {
    if (count > UINT8_MAX) {
      throw std::runtime_error("invalid arguments: stacktrace record count must be less than 256");
    }
    compiler_.setStacktraceRecordCount(static_cast<uint8_t>(count));
  }

  ManagedBinary compile(pybind11::bytes const &script) {
    std::string_view const view = static_cast<std::string_view>(script);
    std::vector<vb::NativeSymbol> nativeSymbols{};
    registerApi("env", "log", "(ii)");
    for (DynNativeSymbolStorage const &symbolStorage : nativeSymbolStorage_) {
      nativeSymbols.emplace_back(vb::NativeSymbol{
          vb::NativeSymbol::Linkage::DYNAMIC,
          symbolStorage.moduleName.c_str(),
          symbolStorage.symbolName.c_str(),
          symbolStorage.signature.c_str(),
          nullptr,
      });
    }
    return compiler_.compile(Span<const uint8_t>{vb::pCast<uint8_t const *>(view.data()), view.size()}, nativeSymbols);
  }

  std::string disassembleWasm(pybind11::bytes const &script) {
    return disassembleModule(compile(script));
  }
  std::string disassembleModule(ManagedBinary const &module) {
    std::stringstream os;
    std::vector<uint32_t> const instructionAddresses = (dwarfGenerator_ != nullptr) ? dwarfGenerator_->getInstructions() : std::vector<uint32_t>{};
    os << disassembler::disassemble(module, instructionAddresses) << "\n";
    return os.str();
  }

  std::string disassembleDebugMap(ManagedBinary const &module) {
    std::stringstream os;
    os << disassembler::disassembleDebugMap(module) << "\n";
    return os.str();
  }

private:
  Compiler compiler_;
  std::unique_ptr<ILogger> logger_;
  std::vector<DynNativeSymbolStorage> nativeSymbolStorage_;
  std::unique_ptr<extension::Dwarf5Generator> dwarfGenerator_;
  std::unique_ptr<extension::Analytics> analytics_;
};

}; // namespace binding

void binding::bindingCompiler(pybind11::module_ &m) {
  m.def("get_configuration", []() -> std::string {
    return disassembler::getConfiguration();
  });

  pybind11::class_<CompilerWrapper>(m, "Compiler", pybind11::module_local())
      .def(pybind11::init<>())
      .def("compile", &CompilerWrapper::compile)
      .def("enable_log", &CompilerWrapper::setLogger)
#if ENABLE_EXTENSIONS
      .def("enable_analytics", &CompilerWrapper::setAnalytics)
      .def("get_jit_size", &CompilerWrapper::getJitSize)
      .def("get_spills_to_stack", &CompilerWrapper::getSpillsToStackCount)
      .def("get_spills_to_reg", &CompilerWrapper::getSpillsToRegCount)

      .def("enable_dwarf", &CompilerWrapper::setDwarf5Generator)
      .def("dump_dwarf", &CompilerWrapper::dumpDwarf)
      .def("get_dwarf_object", &CompilerWrapper::getDwarfObject)
#endif
      .def("set_stacktrace_record_count", &CompilerWrapper::setStacktraceRecordCount)
      .def("enable_debug_mode", &CompilerWrapper::setDebugMode)
      .def("register_api", &CompilerWrapper::registerApi)
      .def("disassemble_wasm", &CompilerWrapper::disassembleWasm)
      .def("disassemble_module", &CompilerWrapper::disassembleModule)
      .def("disassemble_debug_map", &CompilerWrapper::disassembleDebugMap);

  m.def("enable_color", [](bool isEnabled) {
    disassembler::useColor = isEnabled;
  });
}

} // namespace vb
