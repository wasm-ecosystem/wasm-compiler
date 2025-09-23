<style>
table {
	width:100%;
}
tr td:nth-child(1) {
	width: 8em;
}
tr td:nth-child(2) {
	width: 7em;
}
table:last-of-type tr td {
	width: auto !important;
}
</style>

Example Data    				| Size         	| Description
--------						|--------		|-----------------
&nbsp;|| **Wasm Function Bodies**
```4B 0D 0B ...```				| N (uint8_t[])	| Function body (Machine code), Function address points here (OPBVF0)
```xx xx xx```					| N				| Padding to align to 4B (OPBVF1)
```xx xx xx xx```				| N (uint32_t)	| Size of the function body (OPBVF2)
```7C 3A 1D ...```				| N (uint8_t[])	| Function body (Machine code), Function address points here (OPBVF0)
```xx```						| N				| Padding to align to 4B (OPBVF1)
```xx xx xx xx```				| N (uint32_t)	| Size of the function body (OPBVF2)
&nbsp;|| **Data**
```3D 08 0A ...``` 				| N (uint8_t[])	| Initial data binary values (Linear memory) (OPBVLM0)
```xx xx```						| N				| Padding to align to 4B (OPBVLM1)
```00 00 00 16```				| 4 (uint32_t)  | Data segment size (e.g. 0x16 = 22 bytes) (OPBVLM2)
```00 00 00 04```				| 4 (uint32_t)  | Data segment start (e.g. 0x04 = 4th byte of linear memory) (OPBVLM3)
```00 00 00 01```				| 4 (uint32_t)  | Number of data segments (OPBVLM4)
&nbsp;|| **Function Names**
```eval```						| N				| Function name (OPBFN0)
``` ```							| N				| Padding to align to 4B (OPBFN1)
```00 00 00 04```				| 4 (uint32_t)  | Name length (OPBFN2)
```00 00 00 07```				| 4 (uint32_t)  | Function index (OPBFN3)
```00 00 00 01```				| 4 (uint32_t)  | Number of function names (OPBFN4)
```00 00 00 32```				| 4 (uint32_t)  | Section size excl. this (size) (OPBFN5)
&nbsp;|| **Start Function**
```ab cd ef ...```				| N (uint8_t[]) | Function call wrapper (OPBVSF0)
```xx xx```						| N				| Padding to align to 4B (OPBVSF1)
```00 00 00 38```				| 4 (uint32_t)  | Function call wrapper size (OPBVSF2)
```(ifff)F``` 					| N (char[])	| Function signature (OPBVSF3)
```xx```						| N				| Padding to align to 4B (OPBVSF4)
```00 00 00 07```				| 4 (uint32_t)  | Function signature length (OPBVSF5)
```00 00 00 32```				| 4 (uint32_t)  | Section size excl. this (size) (OPBVSF6) (has start function means != 0)
&nbsp;|| **Mutable Native Wasm Globals**
```FF FF FF FF```				| 4 (float) 	| Initial value of global (Size depending on type: i=4, I=8, f=4, F=4) (OPBVNG0)
```00 00 00 00```				| 4 (uint32_t)	| Offset in link data (OPBVNG1)
```i```							| 1 (uint8_t)  	| Type of global (OPBVNG2)
```xx```						| 3				| Padding to align to 4B (OPBVNG3)
```00 00 00 0```				| 4 (uint32_t)  | Number of globals (OPBVNG4)
```00 00 00 32```				| 4 (uint32_t)  | Section size excl. this (size) (OPBVNG5)
&nbsp;|| **Dynamically Imported Functions**
```00 00 00 00```				| 4 (uint32_t)	| Offset in link data (OPBVIF0)
```(i)i```						| N (char[])  	| Function signature (OPBVIF1)
``` ```							| N 			| Padding to align to 4B (OPBVIF2)
```00 00 00 04```				| 4 (uint32_t)  | Signature length (OPBVIF3)
```sqrtss``` 					| N (char[])	| Import name (OPBVIF4)
```xx xx```						| N				| Padding to align to 4B (OPBVIF5)
```00 00 00 06```				| 4 (uint32_t)  | Import name length (OPBVIF6)
```env``` 						| N (char[])	| Module name (OPBVIF7)
```xx```						| N				| Padding to align to 4B (OPBVIF8)
```00 00 00 03```				| 4 (uint32_t)  | Module name length (OPBVIF9)
```00 00 00 02```				| 4 (uint32_t)  | Number of dynamically imported functions (OPBVIF10)
```00 00 00 32```				| 4 (uint32_t)  | Section size excl. this (size) (OPBVIF11)
&nbsp;|| **Memory**
```00 00 00 01```				| 4 (uint32_t)	| Initial memory size (in 64k pages, 0xFFFFFFFF = no memory) (OPBVMEM0)
&nbsp;|| **Exported Globals**
```00 00 00 08```				| 4 (uint32_t)  | Offset in link data (OPBVEG0A OR constant value if immutable (4-8B)) (OPBVEG0B)
```01``` 						| 1 (uint8_t)	| Mutable (OPBVEG1)
```i``` 						| 1 (uint8_t)	| Type (OPBVEG2)
```xx xx```						| 2				| Padding to align to 4B (OPBVEG3)
```glob1``` 					| N (char[])	| Export name (OPBVEG4)
```xx xx xx ```					| N				| Padding to align to 4B (OPBVEG5)
```00 00 00 08```				| 4 (uint32_t)  | Export name length (OPBVEG6)
```00 00 00 01```				| 4 (uint32_t)  | Number of exported globals (OPBVEG7)
```00 00 00 16```				| 4 (uint32_t)  | Section size excl. this (size) (OPBVEG8)
&nbsp;|| **Exported Functions**
```ab cd ef ...```				| N (uint8_t[]) | Function call wrapper (OPBVEF0)
```xx xx```						| N				| Padding to align to 4B (OPBVEF1)
```00 00 00 38```				| 4 (uint32_t)  | Function call wrapper size (OPBVEF2)
```(ifff)F``` 					| N (char[])	| Function signature (OPBVEF3)
```xx```						| N				| Padding to align to 4B (OPBVEF4)
```00 00 00 07```				| 4 (uint32_t)  | Function signature length (OPBVEF5)
```evaluate``` 					| N (char[])	| Export name (OPBVEF6)
``` ```							| N				| Padding to align to 4B (OPBVEF7)
```00 00 00 08```				| 4 (uint32_t)  | Export name length (OPBVEF8)
```00 00 00 00```				| 4 (uint32_t)	| Function index (OPBVEF9)
```00 00 00 01```				| 4 (uint32_t)  | Number of exported functions (OPBVEF12)
```00 00 00 32```				| 4 (uint32_t)  | Section size excl. this (size) (OPBVEF13)
&nbsp;|| **Link Status of Imported Functions**
```00``` 						| 1 (uint8_t)	| Imported Function 1: Link status (0x00 = not linked, 0x01 = linked) (OPBILS1)
```00``` 						| 1 (uint8_t)	| Imported Function 2: Link status (OPBILS1)
```00``` 						| 1 (uint8_t)	| Imported Function 3: Link status (OPBILS1)
```xx```						| N				| Padding to align to 4B (OPBILS2)
```00 00 00 03```				| 4 (uint32_t)  | Number of entries/total number of imported functions (OPBILS3)
&nbsp;|| **Table**
```00 00 00 00```				| 4 (uint32_t)  | Entry 1: Offset from here, 0xFFFFFFFF = undefined, 0x00000000 = not linked (OPBVT0)
```00 00 00 00```				| 4 (uint32_t)  | Entry 1: Functype 0xFFFFFFFF = undefined (OPBVT1)
```00 00 00 00```				| 4 (uint32_t)  | Entry 2: Offset from here (OPBVT0)
```00 00 00 00```				| 4 (uint32_t)  | Entry 2: Functype (OPBVT1)
```00 00 00 00```				| 4 (uint32_t)  | Entry 3: Offset from here (OPBVT0)
```00 00 00 00```				| 4 (uint32_t)  | Entry 3: Functype (OPBVT1)
```00 00 00 03```				| 4 (uint32_t)  | Number of entries (OPBVT2)
&nbsp;|| **Table Function Entry for C++**
```00 00 00 00```				| 4 (uint32_t)  | Entry 1: FunctionOffset (OBBTE1)
```00 00 00 00```				| 4 (uint32_t)  | Entry 2: FunctionOffset (OBBTE1)
```00 00 00 03```				| 4 (uint32_t)  | Number of entries (OBBTE0)
&nbsp;|| **More Info**
```00 00 00 0c```				| 4 (uint32_t)  | Sum of byte-widths of variables kept in the link data. This takes into account globals that are dynamically imported (where the pointer has to be stored in RAM dynamically), mutable globals that are defined in the module and functions that are dynamically imported (OPBVMET0)
```00 00 00 01```				| 4 (uint32_t)  | Offset from here for the landing pad, 0xFFFFFFFF = no landing pad (OPBVMET1)
```00 00 00 01```				| 4 (uint32_t)  | Stacktrace record count (31 bits, MSB is flag whether debugMode is enabled, 1=on, 0=off) (OPBVMET2)
```00 00 00 03```				| 4 (uint32_t)  | Version of this binary module (OPBVER)
```10 37 A0 0B```				| 4 (uint32_t)  | Module binary length excl. this (size) (OPBVMET3)
<br>
