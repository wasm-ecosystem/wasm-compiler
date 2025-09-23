///
/// @file MemUtils.hpp
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
#ifndef MEMUTILS_HPP
#define MEMUTILS_HPP

#include <cstdint>
#include <cstdlib>

#include "src/config.hpp"

namespace vb {

namespace MemUtils {
///
/// @brief get page size of current OS
///
/// @return page size in bytes
///
size_t getOSMemoryPageSize() VB_NOEXCEPT;
///
/// @brief Allocate aligned memory
///
/// @param size memory size to allocate
/// @param alignment
/// @return address of allocated memory. The pointer can't be nullptr
/// @throws std::bad_alloc memory allocation failed
///
uint8_t *allocAlignedMemory(size_t size, size_t const alignment);
///
/// @brief realloc aligned memory
///
/// @param oldCodeStart pointer to realloc
/// @param oldSize old size
/// @param newSize new size
/// @param alignment
/// @return address of new allocated memory. The pointer can't be nullptr
/// @throws std::bad_alloc memory allocation failed
///
uint8_t *reallocAlignedMemory(uint8_t *oldCodeStart, size_t const oldSize, size_t newSize, size_t const alignment);
///
/// @brief free aligned memory
///
/// @param ptr Start address of memory to free
///
void freeAlignedMemory(void *const ptr) VB_NOEXCEPT;
///
/// @brief copy memory and clear instruction cache
///
/// @param dest Memory copy destination
/// @param source Memory copy source
/// @param size size to copy
/// @note clear instruction cache is only done on supported CPU, otherwise clear instruction cache will be skipped, no
/// error.
///
void memcpyAndClearInstrCache(uint8_t *const dest, uint8_t const *const source, size_t const size) VB_NOEXCEPT;

///
/// @brief clean CPU instruction cache
///
/// @param begin start address
/// @param length length to clean
///
/// s This function is only useful on CPU with instruction cache such as aarch64. Otherwise this function do nothing
///
void clearInstructionCache(uint8_t *const begin, size_t const length) VB_NOEXCEPT;

///
/// @brief Set read+write+execute permission to memory
///
/// @param start start address
/// @param len length
/// @return error code. Return 0 if success, otherwise return non-zero
///
int32_t setPermissionRWX(uint8_t *const start, size_t const len) VB_NOEXCEPT;
///
/// @brief Set read+execute permission to memory
///
/// @param start start address
/// @param len length
/// @return error code. Return 0 if success, otherwise return non-zero
///
int32_t setPermissionRX(uint8_t *const start, size_t const len) VB_NOEXCEPT;
///
/// @brief Set read+write permission to memory
///
/// @param start start address
/// @param len length
/// @return error code. Return 0 if success, otherwise return non-zero
///
int32_t setPermissionRW(uint8_t *const start, size_t const len) VB_NOEXCEPT;
///
/// @brief Meta info of memory allocated by mmap
///
struct MmapMemory final {
  uint8_t *ptr; ///< Start address
  int32_t fd;   ///< The mapped file descriptor. Only useful on unix system
};
///
/// @brief Allocate page aligned memory
///
/// @param size size of to be allocated memory
/// @throw std::bad_alloc memory allocation failed
/// @return MmapMemory
///
MmapMemory allocPagedMemory(size_t const size);
///
/// @brief Free page aligned memory
///
/// @param ptr start address to be free
/// @param size size to be free
///
void freePagedMemory(uint8_t *const ptr, size_t const size) VB_NOEXCEPT;
///
/// @brief round size up to OS page size
///
/// @param size
/// @return size_t
///
size_t roundUpToOSMemoryPageSize(size_t const size) VB_NOEXCEPT;
///
/// @brief round size down to OS page size
///
/// @param size
/// @return size_t
///
size_t roundDownToOSMemoryPageSize(size_t const size) VB_NOEXCEPT;

///
/// @brief map a file to memory with read+executable permission
///
/// @param size Size to be mapped
/// @param fd The file descriptor of to be mapped file
/// @throw std::bad_alloc memory allocation failed
/// @return start address of the mapped read executable memory. May return nullptr is memory map failed
///
uint8_t *mapRXMemory(size_t const size, int32_t const fd);
///
/// @brief Stack information
///
///
class StackInfo final {
public:
  void *stackBase = nullptr; ///< Stack base
  void *stackTop = nullptr;  ///< Stack top
  size_t stackSize = 0U;     ///< Stack size
};
///
/// @brief Get the StackInfo of current stack
///
/// @return StackInfo
/// @throws std::runtime_error get stackInfo from OS API failed
///
StackInfo getStackInfo();
///
/// @brief allocate virtual memory by OS API
///
/// @param size memory size which must be aligned to OS page size
/// @throw std::bad_alloc memory allocation failed
/// @return Start address of allocated memory. May return nullptr is allocation failed
///
void *allocVirtualMemory(size_t const size);
///
/// @brief free virtual memory
///
/// @param ptr Start address of memory to be free
/// @param size Size of memory to be free
///
void freeVirtualMemory(void *const ptr, size_t const size) VB_NOEXCEPT;
///
/// @brief commit virtual memory
///
/// @param ptr Start address of memory to be committed
/// @param size Size of memory to be committed
/// @throws std::runtime_error memory commit failed
///
void commitVirtualMemory(void *const ptr, size_t const size);
///
/// @brief uncommit virtual memory
///
/// @param ptr Start address of memory to be uncommitted
/// @param size Size of memory to be uncommitted
/// @throws std::runtime_error memory uncommit failed
///
void uncommitVirtualMemory(void *const ptr, size_t const size);

} // namespace MemUtils

} // namespace vb

#endif /* MEMUTILS_HPP */
