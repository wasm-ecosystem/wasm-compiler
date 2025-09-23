///
/// @file Analytics.cpp
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
#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "Analytics.hpp"

#include "src/core/compiler/common/StackElement.hpp"
#include "src/core/compiler/common/StackType.hpp"

namespace vb {
namespace extension {

void Analytics::updateMemoryUsage(MemoryUsage &memoryUsage, uint32_t const compilerMemorySize) {
  uint32_t const memoryUsageSectionStatic = compilerMemorySize - compilerMemorySizeCurrentSectionStart_;
  memoryUsage.stat = std::max(memoryUsage.stat, memoryUsageSectionStatic);

  if (maxCompilerMemorySizeCurrentSection_ > 0U) {
    uint32_t const memoryUsageSectionDynamic = maxCompilerMemorySizeCurrentSection_ - compilerMemorySize;
    memoryUsage.dyn = std::max(memoryUsage.dyn, memoryUsageSectionDynamic);
  }

  maxCompilerMemorySizeCurrentSection_ = 0U;
  compilerMemorySizeCurrentSectionStart_ = compilerMemorySize;
}

///
/// @brief Print line of a graph
///
/// @param percent Percent of maxBlocks to draw
/// @param maxBlocks Max number of blocks
/// @param end String to print after the line
void printGraphLine(float const percent, uint32_t const maxBlocks, std::string const &end = "\n") {
  constexpr char fullBlock[4] = "█";                                                     // NOLINT(modernize-avoid-c-arrays)
  constexpr std::array<char[4], 8> blockChars = {"", "▏", "▎", "▍", "▌", "▋", "▊", "▉"}; // NOLINT(modernize-avoid-c-arrays)

  uint32_t const fullBlocks = static_cast<uint32_t>(percent * static_cast<float>(maxBlocks));
  uint32_t const eightsBlocks = static_cast<uint32_t>((percent * static_cast<float>(maxBlocks) - static_cast<float>(fullBlocks)) / 0.125F);

  if (fullBlocks == 0U && eightsBlocks == 0U) {
    std::cout << blockChars[1] << end;
  } else {
    for (uint32_t i = 0; i < fullBlocks; i++) {
      std::cout << fullBlock;
    }
    std::cout << blockChars[eightsBlocks] << end;
  }
}

///
/// @brief Print column
///
/// @tparam Type Type of the data to print
/// @param val Value to print
/// @param width Width of the column
/// @param left Whether to justify the text to left (true) or right (false)
template <typename Type> void printColumn(Type const &val, int const width, bool const left = true) {
  std::cout << (left ? std::left : std::right) << std::setw(width) << val;
}

///
/// @brief Format percentage as string
///
/// @param perc Percent
/// @param precision Number of decimal places to print
/// @return Formatted string
std::string percStr(float const perc, int const precision = 1) {
  std::stringstream stream;
  stream << std::fixed << std::setprecision(precision) << perc << "%";
  return stream.str();
}

///
/// @brief Print generic line where title and value are formatted in columns
///
/// @tparam Type Type of the data to print
/// @param title Title
/// @param val Value to print
/// @param post String to print after the line
template <typename Type> void printLine(std::string const &title, Type const &val, std::string const post = "") {
  printColumn(title, 37);
  printColumn(val, 10, false);
  printColumn(" " + post, 10);
  std::cout << std::endl;
}

///
/// @brief Apply ANSI code to a string
///
/// @param str Input string
/// @param ansi String describing the ANSI code(s) to apply, e.g. "1" is bold and "2" is dim
/// @return std::string String with ANSI code applied
std::string applyANSICode(std::string const &str, std::string ansi) {
  return "\033[" + ansi + "m" + str + "\033[0m";
}

void Analytics::printMemoryUsage(std::string const &title, MemoryUsage const &memoryUsage) {
  float const statPerc = 100.F * static_cast<float>(memoryUsage.stat) / static_cast<float>(maxCompilerMemorySize_);

  printColumn(title, 37);
  printColumn(memoryUsage.stat, 10, false);
  printColumn(" (" + percStr(statPerc) + ")", 10);
  float const dynPerc = 100.F * static_cast<float>(memoryUsage.dyn) / static_cast<float>(maxCompilerMemorySize_);
  printColumn(memoryUsage.dyn, 10, false);
  printColumn(" (" + percStr(dynPerc) + ")", 10);
  std::cout << std::endl;
}

void Analytics::printMemoryUsageForSection(std::string const &title, SectionType sectionType) {
  printMemoryUsage(title, maxCompilerMemoryUsagePerSection_[static_cast<uint32_t>(sectionType)]);
}

///
/// @brief Count samples in Histogram
///
/// @tparam N Histogram size
/// @param histogram Histogram data
/// @return uint32_t Total number of samples
template <size_t N> uint32_t countHistogramSamples(std::array<uint32_t, N> histogram) {
  uint32_t totalSamples = 0U;
  for (uint32_t i = 0; i < N; i++) {
    totalSamples += histogram[i];
  }
  return totalSamples;
}

///
/// @brief Print full histogram data
///
/// @tparam N Histogram size
/// @param histogram Histogram data
/// @param printFirstN Number of histogram entries to print
template <size_t N> void printHistogramData(std::array<uint32_t, N> histogram, size_t const printFirstN) {
  uint32_t max = 0U;
  uint32_t const totalSamples = countHistogramSamples(histogram);
  uint32_t shownSamples = 0U;

  for (uint32_t i = 0; i < printFirstN; i++) {
    if ((i < printFirstN) && (histogram[i] > max)) {
      max = histogram[i];
    }
    shownSamples += histogram[i];
  }

  for (uint32_t i = 0; i < printFirstN; i++) {
    std::cout << std::setw(3) << i;
    printGraphLine(static_cast<float>(histogram[i]) / static_cast<float>(max), 60, "");

    float const perc = 100.F * static_cast<float>(histogram[i]) / static_cast<float>(totalSamples);
    std::string const annotation = " " + std::to_string(histogram[i]) + " (" + percStr(perc) + ")";
    std::cout << applyANSICode(annotation, "2") << std::endl;
  }

  if (printFirstN < N) {
    uint32_t const samplesNotShown = totalSamples - shownSamples;
    float const perc = 100.F * static_cast<float>(samplesNotShown) / static_cast<float>(totalSamples);
    std::cout << applyANSICode("... other: " + std::to_string(samplesNotShown) + " (" + percStr(perc) + ")", "2") << std::endl;
  }
}

void Analytics::updateRegPressureHistogram(bool const isGPR, uint32_t const numFreeRegs) {
  if (isGPR) {
    gprPressureHistogram_[numFreeRegs]++;
  } else {
    fprPressureHistogram_[numFreeRegs]++;
  }
}

void Analytics::updateMaxUsedTempStackSlots(uint32_t const usedTempStackSlots, uint32_t const activeTempStackSlots) {
  maxUsedTempStackSlots_ = std::max(maxUsedTempStackSlots_, usedTempStackSlots);
  maxActiveTempStackSlots_ = std::max(activeTempStackSlots, maxActiveTempStackSlots_);

  assert(activeTempStackSlots >= usedTempStackSlots);
  uint32_t const holes = activeTempStackSlots - usedTempStackSlots;
  float const fragmentation = static_cast<float>(holes) / static_cast<float>(activeTempStackSlots);
  avgFragmentation_ = (avgFragmentation_ * static_cast<float>(stackSlotSamples_) + fragmentation) / (static_cast<float>(stackSlotSamples_ + 1U));
  stackSlotSamples_++;

  maxFragmentation_ = std::max(maxFragmentation_, fragmentation);
}

void Analytics::printAnalytics() {
  std::cout << std::fixed << std::setprecision(2);

  //
  // Binary size
  //
  std::cout << std::endl;
  printLine("Input size: ", bytecodeSize_, "bytes");
  printLine("Output size: ", jitSize_, "bytes");
  float const inOutRatio = static_cast<float>(jitSize_) / static_cast<float>(bytecodeSize_);
  printLine("Ratio: ", inOutRatio);
  std::cout << std::endl;

  //
  // Function size
  //
  printLine("Largest function (JIT)", maxFunctionJitSize_, "bytes");
  std::cout << std::endl;

  //
  // Register allocations and spills
  //
  printLine("Max stack frame size", maxStackFrameSize_, "bytes");
  printLine("Max used stack slots", maxUsedTempStackSlots_);
  printLine("Max active stack slots (used + holes)", maxActiveTempStackSlots_);
  printLine("Avg. fragmentation of stack slots", 100.0F * avgFragmentation_, "%");
  printLine("Max fragmentation of stack slots", 100.0F * maxFragmentation_, "%");
  std::cout << std::endl;
  printLine("Spills to stack", spillsToStackCount_);
  printLine("Spills to regs", spillsToRegCount_);
  std::cout << std::endl;

  std::cout << applyANSICode("Number of times during register allocation a specific number of registers was free.", "1") << std::endl;
  std::cout << applyANSICode("Zero means something was stored on/spilled to the stack instead to a register", "2") << std::endl;
  std::cout << applyANSICode("Thus, only the zero number should have a large impact on performance", "2") << std::endl;

  std::cout << "General Purpose Registers (GPR) - Total Allocations: " << countHistogramSamples(gprPressureHistogram_) << std::endl;
  printHistogramData(gprPressureHistogram_, 11U);
  std::cout << std::endl;
  std::cout << "Floating Point Registers (FPR) - Total Allocations: " << countHistogramSamples(fprPressureHistogram_) << std::endl;
  printHistogramData(fprPressureHistogram_, 11U);
  std::cout << std::endl;

  //
  // Memory used during compilation
  //
  printLine("Max compiler memory size", maxCompilerMemorySize_, "bytes");
  size_t const memoryUsedByStackElements = maxStackElementCount_ * sizeof(StackElement);
  printLine("Max StackElements on stack", memoryUsedByStackElements, "bytes (Count " + std::to_string(maxStackElementCount_) + ")");
  std::cout << std::endl;

  std::cout << applyANSICode("Compiler memory usage per section (bytes)", "1") << std::endl;
  std::cout << applyANSICode("Static will be retained after the section end until compilation if finished, dynamic "
                             "will be freed after section end.",
                             "2")
            << std::endl;
  std::cout << applyANSICode("Compiler memory usage at any point is thus the sum of all static usage up to that point "
                             "plus the dynamic usage of the last/current section.",
                             "2")
            << std::endl;
  printColumn("", 37);
  printColumn("Static", 13, false);
  printColumn("", 7);
  printColumn("Dynamic", 13, false);
  printColumn("", 7);
  std::cout << std::endl;

  printMemoryUsageForSection("Custom (max)", SectionType::CUSTOM);
  printMemoryUsageForSection("Type", SectionType::TYPE);
  printMemoryUsageForSection("Import", SectionType::IMPORT);
  printMemoryUsageForSection("Function", SectionType::FUNCTION);
  printMemoryUsageForSection("Table", SectionType::TABLE);
  printMemoryUsageForSection("Memory", SectionType::MEMORY);
  printMemoryUsageForSection("Global", SectionType::GLOBAL);
  printMemoryUsageForSection("Export", SectionType::EXPORT);
  printMemoryUsageForSection("Start", SectionType::START);
  printMemoryUsageForSection("Element", SectionType::ELEMENT);
  printMemoryUsageForSection("Code", SectionType::CODE);
  printMemoryUsageForSection("Data", SectionType::DATA);
  std::cout << std::endl;
  printMemoryUsage("Serialization:", maxMemoryUsageForSerialization_);
  std::cout << std::endl;
}

} // namespace extension
} // namespace vb
