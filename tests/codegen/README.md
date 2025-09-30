# match pattern

## x86_64

- gpr: `[[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]]`
- fpr: `[[REG:xmm[0-9]+]]`
- 8bit: `[[REG:([abcd]l|[bs]pl|[sd]il)]]`

## aarch64

- gpr: `[[REG:w[0-9]+]]` / `[[REG:x[0-9]+]]`
- fpr: `[[REG:[sdv][0-9]+]]`
- address: `0x[[#%x,IDENTIFIER:]]` / `0x[[#%x,IDENTIFIER]]`

## tricore

- gpr `[[REG:d[0-9]+]]` / `[[REG:e[0-9]+]]`
- stack: `[[STACK:\[sp\]#0x[0-9a-f]+]]`
