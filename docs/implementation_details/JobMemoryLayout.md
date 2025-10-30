### Runtime
The link data that both the runtime and the compiler use is structured as follows and contains certain flags representing execution state, pointers to imported objects that are dynamically linked during runtime, Wasm linear memory size, actual linear memory size (for bounds checks)

Example Data    				| Bytes         | Description
--------						|--------		|-----------------
```0B 7C 00 80 3F 00 00 00``` 	| 8 (void *)	| Ptr to end of machine code binary
```0B 7C 00 80 3F 00 00 00``` 	| 8 (void *)	| Ptr to start of machine code binary
```0B 7C 00 80 3F 00 00 00``` 	| 8 (void *)	| Ptr to table section
```0B 7C 00 80 3F 00 00 00``` 	| 8 (void *)	| Ptr of first dynamically linked imported function
```0B 7C 00 80 3F 00 00 00``` 	| 8 (void *)	| Ptr of second dynamically linked imported function
```0B 7C 00 80 3F 00 00 00``` 	| 8 (void *)	| ...
```3F 00 00 00 3F 00 00 00```	| 8 (double)	| Value of first non-imported mutable global
```3F 00 00 00 3F 00 00 00```	| 8 (double)	| Value of second non-imported mutable global
```3F 00 00 00```				| 4 (float)		| Value of third non-imported mutable global
```3F 00 00 00```				| 4 (float)		| ...
```00 00 00 00```				| 4 (char[])	| Padding (optional, depending on whether it's 8-byte aligned at this point)
```10 00 00 FF FF 00 CE 00``` 	| 8 (void*) 	| Pointer to extension request helper
```10 00 00 FF FF 00 CE 00``` 	| 8 (void*) 	| Pointer to job memory object of runtime
```10 00 00 FF FF 00 FF FF``` 	| 8 (uint8_t*) 	| Pointer to external data for high-efficiency handling of boardnet messages.
```10 00 00 AB CD 00 EE FF``` 	| 8 (uint8_t*) 	| Trap stack reentry pointer
```00 00 00 10```				| 4 (uint32_t)  | Actual allocated linear memory size in bytes
```00 00 00 09```				| 4 (uint32_t)	| Current Wasm module linear memory size in multiples of 64kiB (65536B; theoretical maximum until accesses must trap, actual allocated memory size can be smaller)
```00 00 00 01``` 				| 4 (uint32_t) 	| Status flags
```xx xx xx xx``` 				| 4 (char[]) 	| Padding
```11 33 73 ...```				| N (uint8_t[])	| Wasm linear memory
<br>
