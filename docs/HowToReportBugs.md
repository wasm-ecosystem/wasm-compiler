This doc provide guide about bug reporting to debug more efficient.

In case user find any bug about the compiler, please create an issue in github issues and assign it to any [code owner](../.github/CODEOWNERS)

### The issue should contains following info

1. version number
2. Target CPU and Operating system
3. build config. The compiler has several marcos as configuration parameters under [config.hpp](../src/config.hpp). Please provide the build configuration. If default configuration is used, just write "default".
4. Minimal reproduce example. Please refer to following tips to create the minimal reproduce example.
5. What's the observed and Expected behavior

#### Tips about minimal reproduce example.

What means minimal? The "minimal" here is defined in Wasm point of view. For advanced user with solid WebAssembly knowledge, a reproduce example in wat is highly recommended.

For normal users, usually developer may think they observed wrong behavior of their code but the code itself is correct. Usually attaching application code in issues makes debugging difficult, because there is a frontend compiler between user code and JIT compiler. Create minimal reproduce example with high level language is not straightforward for JIT compiler.

Following example cases can help to report bug better:

##### Example 1

When compile following code by AssemblyScript 0.26 and run it, it may crash

```typescript
function f1(a: i32, t: T, b: i32): void {
  trace("f1", 1, t.v);
}

function f2(t: T): i32 {
  __collect();
  let c = new T();
  c.v = 100;
  return 1;
}

export function _start(): void {
  f1(1, new T(), f2(new T()));
}
```

But it caused by a [bug in AssemblyScript](https://github.com/AssemblyScript/assemblyscript/issues/2719) compiler, not in JIT compiler.

To verify this kind case, user can try to execute the Wasm code in another WebAssembly runtime such as nodejs, browsers. If the Wasm code crashes in all the runtimes, it's a frontend compiler bug, not JIT compiler bug.

##### Example 2

In C++ code point of view, the follow code is already simple.

```cpp

#include <cstdint>
#include <cstring>

__attribute__((import_module("env"), import_name("api1"))) void
api1(char const *stringPtr, unsigned int stringLength) noexcept;

__attribute__((import_module("env"), import_name("api2"))) uint32_t
api2(uint32_t p1, void const *p2, uint32_t p3);

void foo(const char *message) { api1(message, strlen(message)); }

extern "C" void _start() {
  static constexpr uint32_t p1{10U};
  static const char *p2{"Some message..."};

  foo(p2);
  for (uint32_t i = 0U; i < p1; i++) {
    foo(p2);
    const uint32_t result = api2(p1, p2, strlen(p2));

    if (result == 0U) {
      const char *error_message{"api call failed"};
      api1(error_message, strlen(error_message));
    }
  }
}
```

But in JIT compiler point of view, this code is not simple. The code contains:

1. if else statement
2. for statement
3. linear memory access
4. global access
5. Wasm internal function call
6. Native function call

So it's difficult to know which part of the JIT compiler goes wrong.
Just assume if the bug is in about native function call, a minimal reproduce example should be

```cpp
__attribute__((import_module("env"), import_name("api1"))) void
api1(char const *stringPtr, unsigned int stringLength) noexcept;

extern "C" void _start() {

  const char *error_message{"api call failed"};
  api1(error_message, 16);
}
```
