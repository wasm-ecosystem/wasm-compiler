# Condense Comparison

## What is Comparison?

```c
a == b
```

### instruction view (aarch64)

`CMP` is an alias of `SUBS`.  
`CMP w0, W1` is same as `SUBS wzr, w0, w1`.  
`cmp w0 w1` means `w0` register subtracts `w1` register. It will update the condition flags based on result and discard the result.

| Condition Flags | Meaning      | Notes                   |
| --------------- | ------------ | ----------------------- |
| EQ              | equal        | Equal                   |
| NE              | not equal    | Not equal               |
| CS              | carry set    | Carry set               |
| HS              | high or same | Unsigned higher or same |

- _NOTE: incomplete_

### programming view

Comparison is special case from programming view.
There are two different operation which have the same name but different semantics.

- value producing comparison

  ```python
  ret = a == b # return 0 or 1
  ```

- control flow comparison

  ```python
  def eq(a, b):
    if a == b: # decide control flow
      # do something
    else
      # do something
  ```

## How to Implement Comparison in Compiler

### Value Producing Comparison

We can condense **value producing comparison** as other operator. It is same as other operator such as add, mul which receive two arguments and return one results.

```wat
local.get 0 [w0]        condense to           
local.get 1 [w1]       =============>         
i32.eq                                        REG [w2]
```

with following emitted machine code.

```asm
00        cmp w0, w1
04        b.ne  #0x10
08        mov  w2, #1
0c        b  #0x14
10        mov  w2, #0
14        ......
```

#### Optimize

conditional move instruction can simplify the code.
`cset` sets the destination register to 1 if the condition is TRUE, otherwise sets it to 0

```asm
00        cmp w0, w1
04        cset  w2, eq
```

### Control Flow Comparison

Condensing **Control Flow Comparison** is different.

There are no real result in register, instead, condition flags are set.
Because jump instructions don't rely on the value in register, instead, they rely on the condition flags to decide control flow.

```wat
local.get 0 [w0]        condense to           
local.get 1 [w1]       =============>         
i32.eq                                        (empty)
```

```asm
          cmp w0, w1
          b.ne <.else>
          // then block
          //  ......
          b  <.end>
.else:
          // else block
          // ......
.end:
          ......
```

## Example of Comparison and Loop

```ts
export function foo(): i32 {
  let v = 0;
  for (let i = 0; i < 10; i++) {
    v += i;
  }
  return v;
}
```

[step by step JIT code](./LoopExample.html)
