All offsets are given as offsets from linear memory in job memory:

version_debugMap: uint32
offset_lastFramePtr: uint32
offset_actualLinMemSize: uint32
offset_trapHandlerPtr: uint32
offset_linkDataStart: uint32
count_mutableGlobals: uint32
mutableGlobals[count_mutableGlobals]: {
    globalIndex: uint32
    offset_inLinkData: uint32
}
count_nonImportedFunctions: uint32
debugInfo_nonImportedFunctions[count_nonImportedFunctions]: {
    fncIndex: uint32
    count_locals: uint32
    locals_frame_offsets[num_locals]: uint32
    count_sourceMap: uint32
    sourceMap[count_sourceMap]: {
        sourceOffset: uint32,
        jitOffset: uint32,
    }
}


<-- Direction of stack growth 
                   ______________________________________________________________________________________________
                   |                                                                                            |
                   ^                                                                                            v
... | next info (void *) | fnc idx (U32) | offset to locals (U32) | pos of caller instr in bytecode (U32) | ... | next info (void *) | fnc idx (U32) | ...
    ^
    |
last frame (ptr in job memory)


uint32_t getFncIndex(uint32_t level)
uint32_t getPosLastCallInstr(uint32_t level)
uint32_t getLocalValueI32(uint32_t localIndex, uint32_t level)
uint64_t getLocalValueI64(uint32_t localIndex, uint32_t level)
uint32_t getGlobalValueI32(uint32_t globalIndex, uint32_t level)
uint64_t getGlobalValueI32(uint32_t globalIndex, uint32_t level)
