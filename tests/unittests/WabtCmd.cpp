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

#include <array>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <sstream>

#include "WabtCmd.hpp"

std::array<const char *, 5> const commandArgs = {"wat2wasm", "--debug-names", "-", "-d", nullptr};

#ifdef _WIN32
#include "src/utils/windows_clean.hpp"
void runWat2Wasm(std::string_view const input, std::string &output) {
  // Create pipes for stdin and stdout redirection
  HANDLE hStdinRead;
  HANDLE hStdinWrite;
  HANDLE hStdoutRead;
  HANDLE hStdoutWrite;
  SECURITY_ATTRIBUTES saAttr = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};

  // Create a pipe for the child process's stdin
  if (CreatePipe(&hStdinRead, &hStdinWrite, &saAttr, 0) == 0) {
    std::cerr << "Stdin pipe creation failed\n";
    return;
  }

  // Ensure the write handle to the pipe for stdin is not inherited
  if (SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0) == 0) {
    std::cerr << "Stdin write handle inheritance failed\n";
    return;
  }

  // Create a pipe for the child process's stdout
  if (CreatePipe(&hStdoutRead, &hStdoutWrite, &saAttr, 0) == 0) {
    std::cerr << "Stdout pipe creation failed\n";
    return;
  }

  // Ensure the read handle to the pipe for stdout is not inherited
  if (SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0) == 0) {
    std::cerr << "Stdout read handle inheritance failed\n";
    return;
  }

  STARTUPINFOA si{};
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = hStdinRead;
  si.hStdOutput = hStdoutWrite;
  si.hStdError = hStdoutWrite;

  // Create the child process
  PROCESS_INFORMATION pi;
  std::stringstream cmdStream;

  for (const char *const arg : commandArgs) {
    if (arg != nullptr) {
      cmdStream << arg << " ";
    }
  }

  std::string command = cmdStream.str();
  if (CreateProcessA(nullptr, &command[0], nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi) == 0) {
    std::cerr << "CreateProcessA failed\n";
    return;
  }

  // Close handles to the stdin and stdout pipes no longer needed by the parent process
  CloseHandle(hStdinRead);
  CloseHandle(hStdoutWrite);

  // Write to the child's stdin
  DWORD written;
  if (WriteFile(hStdinWrite, input.data(), static_cast<DWORD>(input.size()), &written, nullptr) == 0) {
    std::cerr << "Write to stdin failed\n";
    return;
  }
  CloseHandle(hStdinWrite); // Close the write end of the pipe after writing

  // Read from the child's stdout
  std::array<char, 4096U> buffer{};
  DWORD read;
  while (true) {
    BOOL const success = ReadFile(hStdoutRead, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr);
    if (success == 0) {
      break;
    }
    if (read > 0) {
      output.append(buffer.data(), read);
    }
  }
  CloseHandle(hStdoutRead);

  // Wait for the child process to exit
  WaitForSingleObject(pi.hProcess, INFINITE);

  // Close process and thread handles
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
}
#else
#include <sys/wait.h>
#include <unistd.h>
void runWat2Wasm(std::string_view input, std::string &output) {
  std::array<int32_t, 2> stdin_pipe{};
  std::array<int32_t, 2> stdout_pipe{};

  // Create pipes for stdin and stdout redirection
  if (pipe(stdin_pipe.data()) == -1 || pipe(stdout_pipe.data()) == -1) {
    std::cout << "Pipe creation failed" << std::endl;
    return;
  }

  pid_t const pid = fork();
  if (pid == -1) {
    std::cout << "Fork failed" << std::endl;
    return;
  }

  if (pid == 0) { // Child process
    // Redirect stdin
    if (dup2(stdin_pipe[0], STDIN_FILENO) == -1) {
      std::cout << "dup2 stdin failed" << std::endl;
      exit(1);
    }
    close(stdin_pipe[1]);
    close(stdin_pipe[0]);

    // Redirect stdout
    if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1) {
      std::cout << "dup2 stdout failed" << std::endl;
      exit(1);
    }
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);

    // Execute wat2wasm

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    execvp("wat2wasm", const_cast<char *const *>(commandArgs.data()));
    // If execvp fails
    std::cout << "execvp failed\n";
    exit(1);
  } else { // Parent process
    // Close unused pipe ends
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    // Write to the child's stdin
    size_t totalWritten = 0;
    size_t const inputSize = input.size();
    while (totalWritten < inputSize) {
      ssize_t const written = write(stdin_pipe[1], input.data() + totalWritten, static_cast<size_t>(inputSize) - totalWritten);
      if (written == -1) {
        std::cout << "write to stdin failed" << std::endl;
        exit(1);
      }
      totalWritten += static_cast<size_t>(written);
    }
    close(stdin_pipe[1]); // Close the write end of the pipe after writing

    // Read from the child's stdout

    std::array<char, 4096> buffer{};
    ssize_t bytesRead;
    while ((bytesRead = read(stdout_pipe[0], buffer.data(), buffer.size())) > 0) {
      output.append(buffer.data(), static_cast<size_t>(bytesRead));
    }
    if (bytesRead == -1) {
      std::cout << "read from stdout failed" << std::endl;
    }
    close(stdout_pipe[0]);

    // Wait for the child process to exit
    int32_t status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
      int32_t const exitStatus = WEXITSTATUS(status);
      if (exitStatus != 0) {
        std::cout << "Child process exited with status " << exitStatus << std::endl;
        exit(1);
      }
    } else {
      std::cout << "Child process did not exit normally" << std::endl;
      exit(1);
    }
  }
}
#endif

namespace vb {
namespace test {

std::vector<uint8_t> WabtCmd::parseHexDump(const std::string &hexDump) {
  std::vector<uint8_t> result;
  std::istringstream stream(hexDump);
  std::string line;

  while (std::getline(stream, line)) {
    // Ignore lines that don't contain hex data
    if (line.size() < 10U) {
      continue;
    }

    size_t cursor = 9U;

    while (cursor < line.size()) {
      if (line[cursor] == ' ') {
        cursor += 1U;
      } else {
        const char *const hexStart = line.c_str() + cursor;
        const char *const hexEnd = hexStart + 2U;
        uint8_t hexValue;
        auto errorCode = std::from_chars(hexStart, hexEnd, hexValue, 16);

        if (errorCode.ec != std::errc()) {
          std::cerr << "Conversion failed" << std::endl;
          exit(1);
        }
        cursor += 2U;
        result.push_back(hexValue);
      }
    }
  }
  return result;
}

std::vector<uint8_t> const WabtCmd::loadWasmFromWat(std::string_view const watStr) {
  std::string output;
  runWat2Wasm(watStr, output);

  return parseHexDump(output);
}
} // namespace test
} // namespace vb