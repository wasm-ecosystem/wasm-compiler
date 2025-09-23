///
/// @file MemUtils.cpp
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
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "MemUtils.hpp"
#include "OSAPIChecker.hpp"

#include "src/config.hpp"

#ifdef VB_WIN32_OR_POSIX
#include <iostream>
#include <ostream>
#include <sstream>
#endif

#ifdef VB_POSIX
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#ifdef __linux__
// coverity[autosar_cpp14_a16_2_2_violation]
#include <linux/memfd.h>
#include <sys/syscall.h>
#endif

#ifdef __QNX__
#include <fcntl.h>
#include <sys/procfs.h>

#include "src/core/common/VbExceptions.hpp"
#endif

#elif defined(VB_WIN32)
//clang-format off
#include <malloc.h>

#include "windows_clean.hpp"
//clang-format on
#else

#endif

#include "src/core/common/util.hpp"

namespace vb {

namespace MemUtils {

#ifdef JIT_TARGET_AARCH64

///
/// @brief clear instruction cache
///
/// @param begin start address
/// @param length
///
static void clearInstructionCacheImpl(uint8_t *const begin, size_t const length) VB_NOEXCEPT;
#endif

#ifdef VB_WIN32_OR_POSIX
///
/// @brief set memory page permission
///
/// @param start start address
/// @param len length to set in bytes
/// @param permission combination of read, write and executable
/// @return return 0 if success otherwise return none-zero
///
static int32_t setPagePermission(uint8_t *const start, size_t len, int32_t const permission) VB_NOEXCEPT;
///
/// @brief allocate aligned memory without checking return
///
/// @param alignment
/// @param size
/// @return address of the allocated memory. May return nullptr
///
static uint8_t *allocRawAlignedMemory(size_t const alignment, size_t const size) VB_NOEXCEPT;
///
/// @brief reduce align memory
///
/// @param ptr Start address of to be reduced memory
/// @param size New size
/// @param alignment
/// @return new address
///
static void *alignedReduce(void *const ptr, size_t const size, size_t const alignment) VB_NOEXCEPT;

#ifdef VB_POSIX
///
/// @brief Struct read write executable permission of memory
///
struct MEMORY_PERMISSION final {
  ///
  /// @brief read+write+executable permission
  ///
  static constexpr uint32_t MEMORY_READ_WRITE_EXECUTE{
      // coverity[autosar_cpp14_a16_2_3_violation]
      static_cast<uint32_t>(PROT_EXEC) | static_cast<uint32_t>(PROT_READ) | static_cast<uint32_t>(PROT_WRITE)};
  ///
  /// @brief read+executable permission
  ///
  // coverity[autosar_cpp14_a16_2_3_violation]
  static constexpr uint32_t MEMORY_READ_EXECUTE{static_cast<uint32_t>(PROT_EXEC) | static_cast<uint32_t>(PROT_READ)};
  ///
  /// @brief read+write permission
  ///
  // coverity[autosar_cpp14_a16_2_3_violation]
  static constexpr uint32_t MEMORY_READ_WRITE{static_cast<uint32_t>(PROT_READ) | static_cast<uint32_t>(PROT_WRITE)};
};
///
/// @brief get OS memory page size on unix
///
/// @return size_t
///
static size_t getOSMemoryPageSizeImpl() VB_NOEXCEPT {
  // coverity[autosar_cpp14_a16_2_3_violation]
  return static_cast<size_t>(sysconf(_SC_PAGE_SIZE));
}

static int32_t setPagePermission(uint8_t *const start, size_t len, int32_t const permission) VB_NOEXCEPT {
  len = roundUpToOSMemoryPageSize(len);
  int32_t const res{mprotect(start, len, permission)};
  if (res != 0) {
    return 1;
  } else {
    return 0;
  }
}

static uint8_t *allocRawAlignedMemory(size_t const alignment, size_t const size) VB_NOEXCEPT {
  void *buffer;
  int32_t const error{posix_memalign(pCast<void **>(&buffer), alignment, size)};

  if (error == 0) {
    return pCast<uint8_t *>(buffer);
  } else {
    std::cout << "posix_memalign Failed with error number" << error << &std::endl;
    return nullptr;
  }
}

static void *alignedReduce(void *const ptr, size_t const size, size_t const alignment) VB_NOEXCEPT {
  static_cast<void>(alignment);
  return realloc(ptr, size);
}

/// @brief wrapper of mmap, throw std::bad_alloc when mmap failed
/// @param addr bypass to mmap
/// @param length bypass to mmap
/// @param port bypass to mmap
/// @param flags bypass to mmap
/// @param fd bypass to mmap
/// @param offset bypass to mmap
/// @return result of mmap when mmap success
/// @throw throw std::bad_alloc when mmap failed
static uint8_t *wrapperMmap(void *const addr, size_t const length, int32_t const port, int32_t const flags, int32_t const fd, off_t const offset) {
  // coverity[autosar_cpp14_a16_2_3_violation]
  void *const ptr{mmap(addr, length, port, flags, fd, offset)};
  // coverity[autosar_cpp14_a16_2_3_violation]
  // coverity[autosar_cpp14_a5_2_2_violation]
  // coverity[autosar_cpp14_m5_2_9_violation]
  if (ptr != MAP_FAILED) {
    return pCast<uint8_t *>(ptr);
  } else {
    throw std::bad_alloc();
  }
}

uint8_t *mapRXMemory(size_t const size, int32_t const fd) {
  // coverity[autosar_cpp14_a16_2_3_violation]
  constexpr uint32_t prot{static_cast<uint32_t>(PROT_READ) | static_cast<uint32_t>(PROT_EXEC)};
  // coverity[autosar_cpp14_a16_2_3_violation]
  return wrapperMmap(nullptr, roundUpToOSMemoryPageSize(size), static_cast<int32_t>(prot), MAP_PRIVATE, fd, 0);
}

void freeAlignedMemory(void *const ptr) VB_NOEXCEPT {
  free(ptr);
}

#ifdef JIT_TARGET_AARCH64
static void clearInstructionCacheImpl(uint8_t *const begin, size_t const length) VB_NOEXCEPT {
  __builtin___clear_cache(pCast<char *>(begin), pCast<char *>(pAddI(begin, length)));
}
#endif

#elif (defined VB_WIN32)

///
/// @brief Struct read write executable permission of memory
///
struct MEMORY_PERMISSION final {
  ///
  /// @brief read+write+executable permission
  ///
  static constexpr int32_t MEMORY_READ_WRITE_EXECUTE = PAGE_EXECUTE_READWRITE;
  ///
  /// @brief read+executable permission
  ///
  static constexpr int32_t MEMORY_READ_EXECUTE = PAGE_EXECUTE_READ;
  ///
  /// @brief read+write permission
  ///
  static constexpr int32_t MEMORY_READ_WRITE = PAGE_READWRITE;
};
///
/// @brief get OS memory page size on window
///
/// @return size_t
///
size_t getOSMemoryPageSizeImpl() VB_NOEXCEPT {
  SYSTEM_INFO sysInfo;
  GetSystemInfo(&sysInfo);

  return sysInfo.dwPageSize;
}

static int32_t setPagePermission(uint8_t *start, size_t len, int32_t permission) VB_NOEXCEPT {
  len = roundUpToOSMemoryPageSize(len);
  DWORD const flNewProtect = static_cast<DWORD>(permission);
  DWORD flOldProtect;
  int32_t const success = VirtualProtect(start, len, flNewProtect, &flOldProtect);
  if (success != 0) {
    return 0;
  } else {
    uint32_t const errorCode = GetLastError();
    std::cout << "VirtualProtect error failed with error code " << errorCode << std::endl;
    perror("VirtualProtect error");
    return 1;
  }
}

static uint8_t *allocRawAlignedMemory(size_t alignment, size_t size) VB_NOEXCEPT {
  return pCast<uint8_t *>(_aligned_malloc(size, alignment));
}

void *alignedReduce(void *ptr, size_t size, size_t alignment) VB_NOEXCEPT {
  return _aligned_realloc(ptr, size, alignment);
}

void freeAlignedMemory(void *ptr) VB_NOEXCEPT {
  _aligned_free(ptr);
}

#ifdef JIT_TARGET_AARCH64
static void clearInstructionCacheImpl(uint8_t *const begin, size_t const length) VB_NOEXCEPT {
  FlushInstructionCache(GetCurrentProcess(), begin, length);
}
#endif

#else

#endif

size_t getOSMemoryPageSize() VB_NOEXCEPT {
  static std::atomic<size_t> pageSizeCache{0U};
  if (pageSizeCache == 0U) {
    pageSizeCache = getOSMemoryPageSizeImpl();
  }
  return pageSizeCache;
}

int32_t setPermissionRWX(uint8_t *const start, size_t const len) VB_NOEXCEPT {
  return setPagePermission(start, len, static_cast<int32_t>(MEMORY_PERMISSION::MEMORY_READ_WRITE_EXECUTE));
}
int32_t setPermissionRX(uint8_t *const start, size_t const len) VB_NOEXCEPT {
  return setPagePermission(start, len, static_cast<int32_t>(MEMORY_PERMISSION::MEMORY_READ_EXECUTE));
}
int32_t setPermissionRW(uint8_t *const start, size_t const len) VB_NOEXCEPT {
  return setPagePermission(start, len, static_cast<int32_t>(MEMORY_PERMISSION::MEMORY_READ_WRITE));
}

///
/// @brief calculate round up to 2^n alignment
///
/// @param value value before round up
/// @param alignment
/// @return size_t Aligned value
///
static inline size_t roundUp(size_t const value, size_t const alignment) VB_NOEXCEPT {
  assert((alignment & (alignment - 1U)) == 0U && "align not a power of two");
  return ((value + alignment) - 1U) & ~(alignment - 1U);
}
///
/// @brief calculate round down to 2^n alignment
///
/// @param value value before round up
/// @param alignment
/// @return size_t Aligned value
///
static inline size_t roundDown(size_t const value, size_t const alignment) VB_NOEXCEPT {
  assert((alignment & (alignment - 1U)) == 0U && "align not a power of two");
  return value & ~(alignment - 1U);
}

size_t roundUpToOSMemoryPageSize(size_t const size) VB_NOEXCEPT {
  size_t const pageSize{getOSMemoryPageSize()};
  return roundUp(size, pageSize);
}

size_t roundDownToOSMemoryPageSize(size_t const size) VB_NOEXCEPT {
  size_t const pageSize{getOSMemoryPageSize()};
  return roundDown(size, pageSize);
}

uint8_t *allocAlignedMemory(size_t size, size_t const alignment) {
  size = roundUpToOSMemoryPageSize(size);
  uint8_t *const codeStart{pCast<uint8_t *>(allocRawAlignedMemory(alignment, size))};

  if (codeStart == nullptr) {
    throw std::bad_alloc();
  }

  return codeStart;
}

uint8_t *reallocAlignedMemory(uint8_t *oldCodeStart, size_t const oldSize, size_t newSize, size_t const alignment) {
  uint8_t *newCodeStart;
  newSize = roundUpToOSMemoryPageSize(newSize);

  if (oldSize == newSize) {
    return oldCodeStart;
  } else if (newSize < oldSize) {
    newCodeStart = pCast<uint8_t *>(alignedReduce(oldCodeStart, newSize, alignment));
    if (newCodeStart == oldCodeStart) {
      return newCodeStart;
    } else {
      oldCodeStart = newCodeStart; // Corner case, address changed when reduce size
    }
  } else {
    static_cast<void>(0);
  }

  newCodeStart = allocAlignedMemory(newSize, alignment);

  if (oldCodeStart != nullptr) {
    static_cast<void>(std::memcpy(newCodeStart, oldCodeStart, std::min<size_t>(oldSize, newSize)));
    freeAlignedMemory(oldCodeStart);
  }

  return newCodeStart;
}

// coverity[autosar_cpp14_a15_5_3_violation]
// coverity[autosar_cpp14_m15_3_4_violation]
void memcpyAndClearInstrCache(uint8_t *const dest, uint8_t const *const source, size_t const size) VB_NOEXCEPT {
  static_cast<void>(std::memcpy(dest, source, size));
#if (defined __linux__) && defined(NVALGRIND)
  // flush the machine code to memfd for later rx mapping
  static_assert(sizeof(size_t) == sizeof(uintptr_t), "size_t doest not equal to uintptr_t");
  size_t const destNum{static_cast<size_t>(pToNum(dest))};
  size_t const alignedDestNum{roundDownToOSMemoryPageSize(destNum)};
  void *const alignedDest{numToP<void *>(alignedDestNum)};
  size_t const alignedSize{roundUpToOSMemoryPageSize((destNum - alignedDestNum) + size)};
  // coverity[autosar_cpp14_a16_2_3_violation]
  int32_t const error{msync(alignedDest, alignedSize, MS_SYNC)};
  // coverity[autosar_cpp14_a15_4_2_violation]
  checkSysCallReturn("msync failed", error);
#endif
  clearInstructionCache(dest, size);
}

MmapMemory allocPagedMemory(size_t const size) {
  MmapMemory mmapMemory{nullptr, -1};
#ifdef VB_WIN32
  mmapMemory.ptr = allocAlignedMemory(size, getOSMemoryPageSize());
  return mmapMemory;
#else
  size_t const alignedSize{roundUpToOSMemoryPageSize(size)};
#ifdef __linux__
  static std::atomic<uint64_t> fileCounter{0U}; ///< counter of mmap files
  // coverity[autosar_cpp14_a16_2_3_violation]
  std::stringstream ss{};
  uint64_t const localCounter{fileCounter};
  fileCounter++;
  ss << getpid() << "_vb_wasm_mem" << localCounter;
  int32_t const jitCodeMapFile{
      // coverity[autosar_cpp14_a16_2_3_violation]
      static_cast<int32_t>(syscall(__NR_memfd_create, ss.str().c_str(), MFD_CLOEXEC))}; // NOLINT(cppcoreguidelines-pro-type-vararg)
  if (jitCodeMapFile == -1) {
    return mmapMemory;
  }

  int32_t error{ftruncate(jitCodeMapFile, static_cast<off_t>(alignedSize))};

  if (error != 0) {
    error = close(jitCodeMapFile);
    static_cast<void>(error);
    assert(error == 0 && "close file failed");
    return mmapMemory;
  }
  // coverity[autosar_cpp14_a16_2_3_violation]
  constexpr int32_t flag{MAP_SHARED};
  mmapMemory.fd = jitCodeMapFile;
#else
  mmapMemory.fd = -1;
  constexpr uint32_t uflag = static_cast<uint32_t>(MAP_ANONYMOUS) | static_cast<uint32_t>(MAP_PRIVATE);
  constexpr int32_t flag = static_cast<int32_t>(uflag);
#endif
  // coverity[autosar_cpp14_a16_2_3_violation]
  constexpr uint32_t prot{static_cast<uint32_t>(PROT_READ) | static_cast<uint32_t>(PROT_WRITE)};
  mmapMemory.ptr = wrapperMmap(nullptr, alignedSize, static_cast<int32_t>(prot), static_cast<int32_t>(flag), mmapMemory.fd, 0);

  return mmapMemory;
#endif
}

void freePagedMemory(uint8_t *const ptr, size_t const size) VB_NOEXCEPT {
  if (nullptr == ptr) {
    return;
  }
#ifdef VB_WIN32
  static_cast<void>(size);
  freeAlignedMemory(ptr);
#else
  int32_t const error{munmap(ptr, roundUpToOSMemoryPageSize(size))};
  static_cast<void>(error);
  assert(error == 0 && "munmap failed");
#endif
}

StackInfo getStackInfo() {
  StackInfo stackInfo{};
#ifdef __linux__

  pthread_attr_t attrs;
  pthread_t const threadId{pthread_self()};
  int32_t error{pthread_getattr_np(threadId, &attrs)};
  checkSysCallReturn("pthread_getattr_np", error);
  error = pthread_attr_getstack(&attrs, &stackInfo.stackTop, &stackInfo.stackSize);
  checkSysCallReturn("pthread_attr_getstack", error);
  error = pthread_attr_destroy(&attrs);
  checkSysCallReturn("pthread_attr_destroy", error);
  stackInfo.stackBase = pAddI(pCast<uint8_t *>(stackInfo.stackTop), stackInfo.stackSize);

#elif defined(__APPLE__)
  pthread_t const self = pthread_self();
  stackInfo.stackBase = pthread_get_stackaddr_np(self);
  stackInfo.stackSize = pthread_get_stacksize_np(self);
  stackInfo.stackTop = pSubI(pCast<uint8_t *>(stackInfo.stackBase), stackInfo.stackSize);

#elif defined(VB_WIN32)

#ifdef NEED_CURRENT_TP
  constexpr uint64_t stackTopOffset = 0x10;
#if CXX_TARGET == ISA_X86_64
  void *currentTop = numToP<void *>(__readgsqword(stackTopOffset));
#elif CXX_TARGET == ISA_AARCH64
  void *currentTop = numToP<void *>(__readx18qword(stackTopOffset));
#endif
#endif
  ULONG_PTR stackAllocationTop;
  GetCurrentThreadStackLimits(&stackAllocationTop, pCast<PULONG_PTR>(&stackInfo.stackBase));

  MEMORY_BASIC_INFORMATION uncommitRegion{};
  MEMORY_BASIC_INFORMATION guardRegion{};
  size_t ret = VirtualQuery(numToP<void *>(stackAllocationTop), &uncommitRegion, sizeof(MEMORY_BASIC_INFORMATION));
  checkSysCallReturn("VirtualQuery", ret > 0 ? 0 : 1);
  void *const guardBottom = pCast<uint8_t *>(uncommitRegion.BaseAddress) + uncommitRegion.RegionSize;
  ret = VirtualQuery(guardBottom, &guardRegion, sizeof(MEMORY_BASIC_INFORMATION));
  checkSysCallReturn("VirtualQuery", ret > 0 ? 0 : 1);

  ULONG guaranteeStackSize = 0;
  BOOL const success = SetThreadStackGuarantee(&guaranteeStackSize);
  checkSysCallReturn("SetThreadStackGuarantee", (success > 0) ? 0 : 1);

  // https://techcommunity.microsoft.com/t5/windows-blog-archive/pushing-the-limits-of-windows-processes-and-threads/ba-p/723824
  size_t reservedStackSize = guardRegion.RegionSize + guaranteeStackSize;
  constexpr size_t minimalCommitSize = 0x4000U;
  reservedStackSize = roundUp(reservedStackSize, minimalCommitSize);
  ULONG_PTR const stackTop = stackAllocationTop + reservedStackSize;
  stackInfo.stackTop = numToP<void *>(stackTop);
  stackInfo.stackSize = static_cast<size_t>(pSubAddr(stackInfo.stackBase, stackInfo.stackTop));

#elif defined(__QNX__)
  debug_thread_t threadInfo{};
  threadInfo.tid = pthread_self();
  int32_t fd = open("/proc/self/as", O_RDONLY);
  if (fd == -1) {
    throw RuntimeError(ErrorCode::can_t_open__proc_self_as);
  }
  int32_t error = devctl(fd, DCMD_PROC_TIDSTATUS, &threadInfo, sizeof(threadInfo), 0);
  checkSysCallReturn("devctl", error);
  error = close(fd);
  checkSysCallReturn("close", error);
  stackInfo.stackSize = threadInfo.stksize;
  stackInfo.stackTop = numToP<uint8_t *>(threadInfo.stkbase + 0x1000U); // 1k guard page
  stackInfo.stackBase = numToP<uint8_t *>(threadInfo.stkbase + stackInfo.stackSize);

#else
  static_assert(false, "unsupported OS");
#endif
  return stackInfo;
}

void *allocVirtualMemory(size_t const size) {
  assert(roundUpToOSMemoryPageSize(size) == size && "size must be aligned");
#ifdef VB_WIN32
  return VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
#else
  // coverity[autosar_cpp14_a16_2_3_violation]
  constexpr uint32_t flag{static_cast<uint32_t>(MAP_ANONYMOUS) | static_cast<uint32_t>(MAP_PRIVATE)};
  // coverity[autosar_cpp14_a16_2_3_violation]
  return wrapperMmap(nullptr, size, PROT_NONE, static_cast<int32_t>(flag), -1, 0);
#endif
}

void freeVirtualMemory(void *const ptr, size_t const size) VB_NOEXCEPT {
#ifdef VB_WIN32
  static_cast<void>(size);
  const int32_t success = VirtualFree(ptr, 0, MEM_RELEASE);
  checkSysCallReturn("freeVirtualMemory: VirtualFree", success != 0 ? 0 : 1);
#else
  int32_t const error{munmap(ptr, size)};
  static_cast<void>(error);
  assert(error == 0 && "munmap failed");
#endif
}

void commitVirtualMemory(void *const ptr, size_t const size) {
  assert(roundUpToOSMemoryPageSize(size) == size && "size must be aligned");
#ifdef VB_WIN32
  const void *const addr = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
  const int32_t error = (addr != nullptr) ? 0 : 1;
  checkSysCallReturn("commitVirtualMemory: VirtualAlloc", error);
#else
  int32_t const error{setPermissionRW(pCast<uint8_t *>(ptr), size)};
  checkSysCallReturn("commitVirtualMemory: setPermissionRW", error);
#endif
}

void uncommitVirtualMemory(void *const ptr, size_t const size) {
  assert(roundUpToOSMemoryPageSize(size) == size && "size must be aligned");
#ifdef VB_WIN32
  const int32_t success = VirtualFree(ptr, size, MEM_DECOMMIT);
  checkSysCallReturn("uncommitVirtualMemory: VirtualFree", success != 0 ? 0 : 1);
#else
  // coverity[autosar_cpp14_a16_2_3_violation]
  int32_t error{setPagePermission(pCast<uint8_t *>(ptr), size, PROT_NONE)};
  checkSysCallReturn("uncommitVirtualMemory: setPermissionRW", error);
#ifdef __linux__
  // Don't use posix_madvise on linux, see:
  // https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/unix/sysv/linux/posix_madvise.c;h=71ef3435eaea2e0feb6954468bd1ba1f80f0a6d1;hb=HEAD#l31
  // coverity[autosar_cpp14_a16_2_3_violation]
  error = madvise(ptr, size, MADV_DONTNEED);
#elif defined(__APPLE__) || defined(__QNX__)
  error = posix_madvise(ptr, size, POSIX_MADV_DONTNEED);
#else
  static_assert(false, "not supported OS");
#endif
  checkSysCallReturn("uncommitVirtualMemory: madvise", error);
#endif
}

#endif
// coverity[autosar_cpp14_m0_1_8_violation]
// coverity[autosar_cpp14_m7_1_2_violation] // NOLINTNEXTLINE(readability-non-const-parameter) OS api need char* without const
void clearInstructionCache(uint8_t *const begin, size_t const length) VB_NOEXCEPT {
  static_cast<void>(begin);
  static_cast<void>(length);
#ifdef JIT_TARGET_AARCH64
  clearInstructionCacheImpl(begin, length);
#elif (defined __CPTC__)
  __dsync();
  __isync();
#endif
}

} // namespace MemUtils

} // namespace vb
