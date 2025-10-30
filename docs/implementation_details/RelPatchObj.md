# RelPatchObj

## How to emit a jump instruction which will jump to the future?

```wat
block
  ...
  br_if 0 ;; jump to end
end
```

### Back Patching

```python
if opcode == "br_if":
  target_block.patch_pos = current_pos
  emit("cbz <UNKNOWN ADDRESS>")

if opcode == "end":
  # patch_pos is "cbz <current_pos>"
  patch(target_block.patch_pos, current_pos - target_block.patch_pos)
```

## How to handle multiple instructions?

```wat
block
  br_if 0
  br_if 0
  br_if 0
  br_if 0
end
```

we need a data structure to store all positions!

```python
if opcode == "br_if":
  target_block.patch_pos.push(current_pos)
  emit("cbz <UNKNOWN ADDRESS>")

if opcode == "end":
  # all patch_pos is "cbz <current_pos>"
  for patch_pos in target_block.patch_pos:
    patch(patch_pos, current_pos - patch_pos)
```

### Optimize

The pending patched address can be used to store this data structure.

```
  |--> cbz <UNKNOWN ADDRESS>
  |
  |
  |
  |--- cbz <UNKNOWN ADDRESS> <--|
                                |
                                |
                                |
  |--> cbz <UNKNOWN ADDRESS> ---|
  |
  |
  |
  |--- cbz <UNKNOWN ADDRESS> <------ target_block.patch_pos
```
