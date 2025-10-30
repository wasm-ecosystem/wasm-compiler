# multi-values

## Background

There are a couple of arity restrictions on the WebAssembly that were imposed to simplify the initial MVP WebAssembly release.

- functions can consume multiple operands but can produce at most one result.

  - functions: `value* -> value?`

- instruction like _block , if , loop_ can not consume any stack values, and may only produce zero or one resulting stack value.

  - blocks: `[] -> value?`

## Motivation

- Multiple return values for functions:

  - enable unboxing of tuples or structs returned by value.
  - efficient compilation of multiple return values.

- _block/if/loop_ can consume and produce an arbitrary number of stack values:

  - more compact instructions

There are a few scenarios where compilers are forced to jump through hoops when producing multiple stack values. Workarounds include introducing temporary local variables, and using `local.get` and `local.set` instructions, because the arity restrictions on blocks, this a mean that the values can not be left on the stack.

Consider a scenario where we are computing two stack values: the pointer to a string in linear memory, and it's length. Furthermore, imagine we are choosing between two different strings (which therefore have different pointer-and-length pairs) based on some condition. But whichever string we choose, we are going to process the string in the same fashion, so we just want to push the pointer-and-length pair for our chosen string onto the stack, and control flow can join afterwards.

To implement above requirement, using v1.0 WebAssembly without multi-values support:

```wasm
;; Introduce a pair of locals to hold the values across the instruction sequence boundary.
(local $string i32)
(local $length i32)

call $compute_condition
if
    call $get_first_string_pointer
    local.set $string
    call $get_first_string_length
    local.set $length
else
    call $get_second_string_pointer
    local.set $string
    call $get_second_string_length
    local.set $length
end

;; Restore the values onto the stack, from their temporaries.
local.get $string
local.get $length
```

This encoding requires 30 bytes.

With multi-values, we can do this in a straightforward fashion:

```wasm
call $compute_condition
if (result i32 i32)
    call $get_first_string_pointer
    call $get_first_string_length
else
    call $get_second_string_pointer
    call $get_second_string_length
end
```

This encoding is also compact: only 16 bytes ! An overhead of 14 bytes less than the single-value version. And the additional overhead is proportional to how many values we are producing in the if and else arms.

In summary, without multi value, we have to find some temporary storage area to store the multi values, either in linear memory or by introducing temporary local variables as workaround. And this means we'll generate bigger code, and less efficient.

## How to implement ?

### Binary format

The block type is given as a type use, analogous to the type of functions. However, the special case of a type use that is syntactically empty or consists of only a single result is not regarded as an abbreviation for an inline function type, but is parsed directly into an optional value type.

blocks can have the same set of types that functions can have. Functions already de-duplicate their types in the "Type" section of a Wasm binary and reference them via index. The typeIdx or optional value type is encoded directly in the instruction:

```
blocktype ::=   0x40            => [] -> []
            |   0x7F            => [] -> [i32]
            |   0x7E            => [] -> [i64]
            |   0x7D            => [] -> [f32]
            |   0x7C            => [] -> [f64]
            |   typeIdx         => ft                // introduced by multi-values
```

### multiple return values for function

For the initial MVP WebAssembly, there is only 0 or 1 return value in function, so we could simply store the return value in the return value register as specified by the native ABI. But for multi-values, the question is :

**How to return more values than can fit in registers ?**

In order to support multi-values, we extend the Wasm ABI in wasm-compiler:

1. the first return value will be stored in return value register (Now Wasm ABI support only one return value register), the others stored in the runtime stack.
1. the caller function is responsible for reserving stack slots for return values larger than could fit in return value register.

> Tips: we call the wasm operand stack as compiler stack, and call the operating system call stack as runtime stack.

#### For calling exported functions with multi values from C++ :

In Runtime, we provide a function `invokeWasmWrapperMultiAndCheckTrap(WasmWrapperMulti fncPtr, uint8_t const *const serArgs, uint8_t *const linMemStart, uint8_t *const results)` for calling exported function with multi-values. It passed a pointer to the results to the wrapper function, and In wrapper function, load the multi-return-values to the memory the pointer point to.

### multiple values for blocks

The Wasm ABI is not only work for function call, but also for cases that need to merge control flow:

- params for `loop`
- results for `block`

In case conditional branch, the return values for blocks can be different between different path/basic-block. But after the control flow merged, the follow up instructions should be in consistent status no matter which basic block is the predecessor. For example:

```wasm
(func (param i32 i32) (result i32)
    block $L1 (result i32)
        i32.const 1
        local.get 0
        br_if $L1               ;; if branch to the end, then i32.const 1 is the element on the compiler stack
        drop
        local.get 1             ;; if directly reach the end of block here without branch, the local 1 will remain on the compiler stack
    end
    ...
)
```

To fulfill above requirement that all different conditional control flows to lead to the same return values at the end of each block, we do :

1. before enter a block/loop which has multi-results, need to reserve stack slots for storing results, similar to function call.
1. for different path to the merge point, we emit code to move the compiler stack value to stack slots (first to return value register, the others to runtime stack slot).
1. after merge control flow point, we known that the results must be in the same place, no matter which basic block is the predecessor.

> Note:
>
> 1. For Block with `blocktype: [t1*] -> [t2*]`, when branch to this block, we should consume [t2*], because branch to the block means branch to the block end.
> 1. For Loop with `blocktype: [t1*] -> [t2*]`, when branch to this loop, we should consume [t1*], because branch to the loop means branch to the loop start.

#### load params for Block

There is **no need to merge control flow** while entering a Block. All we need to do is simply **move** the parameters into the `block` on the compiler stack.

But now due to the implement limitations, we have to **copy** the parameter into the `block`,and leave an index (named `blockParamsBaseIndex`) point to the first parameter, then we should truncate the compiler stack below to this index while terminate this block.

> Note: The instructions in the `block` body, is not allowed to access the stack values outside the `block`.

```wasm
(func (param i32 i32) (result i32)
  local.get 0
  local.get 1
  ;; Before block, the compiler stack is [local0, local1]
  (block (param i32 i32) (result i32)
  ;; After entering the block, the compiler stack should be [local0, local1, block, local0, local1]
    i32.add
  )
)
```

#### store results for Block

We **need merge control flow** while terminate the block, must emit the results to the runtime stack, otherwise the subsequent instructions can not get the correct value (see above section).

we do :

1. while entering the `block`, need to reserve stack slots on the runtime stack for results, then store the `blockResultsStackOffset` in the Block StackElement.
1. while met the `end`, first condense the results for this block.
1. emit the condensed results to the stack slots(runtime stack), which reserved before.
1. truncate the compiler stack below the `block` StackElement.
1. finalizeBlock.
1. pop the `block` from the compiler stack.
1. truncate the compiler stack below to the `blockParamsBaseIndex`.
1. push the results (first is ScratchRegister, others are in stack slots) to the compiler stack

#### loop params

We **need merge control flow** while enter the loop, due to `branch to loop` is jump to the start.

So what we do is very similar to block results.

1. entering the `loop`, need to reserve stack slots on the runtime stack for loop params, then store the offset of reserved stack slots in the Loop StackElement.
2. condense the params.
3. emit code for load the params to the stack slots(runtime stack), which reserved before.
4. truncateToBelow the params.
5. push the `loop` stackElement to the compiler stack
6. push the params (first is ScratchRegister, others are in stack slots) to the compiler stack

#### loop results

There is **no need to merge control flow** while terminating a loop, all we need to do is move the results outside the `loop` on the compiler stack. It is similar to load params for block.

The compiler stack changes as follows:

```
1. before the `end` of loop, the compiler stack is: [loop, result1, result2]
1. after terminated the loop, the compiler stack should be: [result1, result2]
```

#### multi-values for if-else

To simplify the handling of `if-else`, if and else is transformed to two blocks and two branches, like [Block, ifBlock].

The following Wasm instruction sequence:

```wasm
local.get 0
if (result i32)
     i32.const 2
else
     i32.const 3
end
```

can be transformed to this equivalent sequence. This way we only need to implement blocks and branches and do not need to implement if-else separately.

```wasm
block (BLOCK)
  block (IFBLOCK)
      local.get 0
      i32.eqz
      br_if 0 (to C)
      i32.const 2
      br 1 (to D)
  end (C)
  i32.const 3
end (D)
```

So load params : [param1, param2] -> [Block, param1, param2, ifBlock, param1, param2]

Store results is very similar to `block`, but be careful about case that no else block:

```wasm
if (param i32 i32) (result i32 i32)
;; before multi-values, no-else means the `if` can not has a result value, otherwise non-balanced stack
;; but after multi-values, no-else case can happen when params == results
;; In this case, we need to store results for ifBlock even it is empty else.
...
end
```

### multi-values for branch instructions

For branch instruction, we should consume the stack values depending on the branch target, and then emit the code to load the stack values to the stack slots (which reserved before, according to the Wasm ABI).

1. branch to block with `blocktype: [t1*] -> [t2*]`, we should consume [t2*], because branch to the block means branch to the block end.
1. branch to loop with `blocktype: [t1*] -> [t2*]`, we should consume [t1*], because branch to the loop means branch to the loop start.
1. branch to function end, means to `return`.

> Note: `br_if` and `br_table` need retain the results (stack element) on the compiler stack, due to they are conditional branch.

#### multi-values for br_table

For `br_table` instruction, we emit code like :

```asm
mov   edi, ebp              // move target index to register
...                         // here need condense the results
lea   rsi, [rip + 0x8]      // load branch table start to register
mov   edi, [rsi + rdi * 4]  // load delta from table start to indexReg by accessing table
add   rsi, rdi              // add the table start and the delta
jmp   rsi                   // jump to the target branch

xxxx                        // here is the jump table, store the delta from table start to branch address
xxxx
xxxx

// branch 1
load the results           // load the results to the stack slots for the target 1 (which reserved before, according to Wasm ABI)
jmp to target 1

// branch 2
load the results           // load the results to the stack slots for the target 2 (which reserved before, according to Wasm ABI)
jmp to target 2

// branch 3
load the results           // load the results to the stack slots for the target 3 (which reserved before, according to Wasm ABI)
jmp to target 3
```
