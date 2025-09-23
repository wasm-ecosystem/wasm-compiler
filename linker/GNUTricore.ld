/* Use custom linker script instead of the default one because the stack size of default linker script is to small */
OUTPUT_FORMAT("elf32-tricore")
OUTPUT_ARCH(tricore)
ENTRY(_start)
/*
 * Define the sizes of the user and system stacks.
 */
__ISTACK_SIZE = DEFINED (__ISTACK_SIZE) ? __ISTACK_SIZE : 4K ;/*1k on TC 39*/
__USTACK__SIZE = 20K; /*experience value of x86_64*/
__CSA_SIZE = DEFINED(__CSA_SIZE) ? __CSA_SIZE : 4K ; /*16K on TC 39*/
/*
 * The heap is the space which is used by malloc() and various other
 * functions. Below you can define the minimum amount of memory that
 * the heap should provide.
 */
__HEAP__SIZE = 2M ;/*set as large as possible*/

MEMORY
{
  QEMU_RAM (w!xp): org = 0xd0000000, len = 3M
}

MEMORY
{
  DATA_RAM (w!xp): org = 0xd0300000, len = 5M
}

MEMORY
{
  LARGE_ROM (w!xp): org = 0x80300000, len = 5M
}

REGION_ALIAS("LARGE_DRAM", QEMU_RAM)

REGION_ALIAS("DATA_MEM2", DATA_RAM)
REGION_ALIAS("SDATA_MEM2", LARGE_DRAM)
REGION_ALIAS("BSS_MEM2", LARGE_DRAM)
REGION_ALIAS("ZDATA_MEM2", LARGE_DRAM)
REGION_ALIAS("CSA_MEM2", LARGE_DRAM)

SECTIONS
{
  /*
   * The startup code should be placed where the CPU expects it after a reset,
   * so we try to locate it first, no matter where it appears in the list of
   * objects and libraries (note: because the wildcard pattern doesn't match
   * directories, we'll try to find crt0.o in various (sub)directories).
   */
  .startup :
  {
    KEEP (*(.startup_code))
        . = ALIGN(8);
  } > CODE_MEM =0
  /*
   * Allocate space for absolute addressable sections; this requires that
   * "ZDATA_MEM" starts at a TriCore segment (256M) and points to
   * some RAM area!  If these conditions are not met by your particular
   * hardware setup, you should either not use absolute data, or you
   * must move .zdata*,.zbss*,.bdata*,.bbss* input sections to some appropriate
   * memory area.
   */
 CORE_SEC(.zbss)  : ALIGN(8) FLAGS(azw)
  {
    CORE_SYM(ZBSS_BASE) = . ;
    *(.zbss)
    *(.zbss.*)
    *(.gnu.linkonce.zb.*)
        *(.bbss)
    *(.bbss.*)
    CORE_SYM(ZBSS_END) = . ;
  } > ZDATA_MEM2
  CORE_SEC(.zdata)  : ALIGN(8) FLAGS(awzl)
  {
    CORE_SYM(ZDATA_BASE) = . ;
   *(.zdata)
        *(.zdata.*)
    *(.gnu.linkonce.z.*)
    *(.bdata)
    *(.bdata.*)
    CORE_SYM(ZDATA_END) = . ;
  } > ZDATA_MEM2 AT> CODE_MEM
  CORE_SEC(.zrodata)  : ALIGN(8) FLAGS(arzl)
  {
        *(.zrodata)
        *(.zrodata.*)
    *(.gnu.linkonce.zr.*)
  } > ZDATA_MEM2 AT> CODE_MEM

  /*
   * Allocate trap and interrupt vector tables.
   */
  CORE_SEC(.traptab)  : ALIGN(8)
  {
    *(.traptab)
  } > CODE_MEM
  CORE_SEC(.inttab)  : ALIGN(8)
  {
    *(.inttab)
  } > CODE_MEM
  CORE_SEC(.init)  : ALIGN(8) FLAGS(axl)
  {
      /* we have to keep all .init/.fini section
         because the linker should not delete this
         sections with --gc-sections
       */
        KEEP( *(SORT(.init*)) )
        KEEP( *(SORT(.fini*)) )
  } > CODE_MEM =0
  /*
   * Allocate .text and other read-only sections.
   */
  CORE_SEC(.text)  : ALIGN(8) FLAGS(axl)
  {
    *(.text)
        *(.text.*)
    *(.pcp_c_ptr_init)
    *(.pcp_c_ptr_init.*)
    *(.gnu.linkonce.t.*)
    /*
     * .gnu.warning sections are handled specially by elf32.em.
     */
    *(.gnu.warning)

  } > CODE_MEM =0
  CORE_SEC(.rodata)   : ALIGN(8) FLAGS(arl)
  {
    *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
    . = ALIGN(8);
  } > CODE_MEM
  CORE_SEC(.clear_sec) : ALIGN(8) FLAGS(arl)
  {
    /*
     * Create the clear and copy tables that tell the startup code
     * which memory areas to clear and to copy, respectively.
     */
    PROVIDE(CORE_SYM(__clear_table) = .) ;
    LONG(0 + ADDR(CORE_SEC(.bss)));     LONG(SIZEOF(CORE_SEC(.bss)));
    LONG(0 + ADDR(CORE_SEC(.sbss)));    LONG(SIZEOF(CORE_SEC(.sbss)));
    LONG(0 + ADDR(CORE_SEC(.zbss)));    LONG(SIZEOF(CORE_SEC(.zbss)));
    LONG(__PCP_CODE_RAM_BEGIN);    LONG(__PCP_CODE_RAM_SIZE);
    LONG(-1);                 LONG(-1);
  } > CODE_MEM
  CORE_SEC(.copy_sec) : ALIGN(8) FLAGS(arl)
  {
    PROVIDE(CORE_SYM(__copy_table) = .) ;
    LONG(LOADADDR(CORE_SEC(.data)));    LONG(0 + ADDR(CORE_SEC(.data))); LONG(SIZEOF(CORE_SEC(.data)));
    LONG(LOADADDR(CORE_SEC(.sdata)));   LONG(0 + ADDR(CORE_SEC(.sdata)));LONG(SIZEOF(CORE_SEC(.sdata)));
    LONG(LOADADDR(CORE_SEC(.zdata)));   LONG(0 + ADDR(CORE_SEC(.zdata)));LONG(SIZEOF(CORE_SEC(.zdata)));
    LONG(LOADADDR(CORE_SEC(.zrodata))); LONG(0 + ADDR(CORE_SEC(.zrodata)));LONG(SIZEOF(CORE_SEC(.zrodata)));
    LONG(LOADADDR(.pcpdata)); LONG(0 + ADDR(.pcpdata)); LONG(SIZEOF(.pcpdata));
    LONG(LOADADDR(.pcptext)); LONG(0 + ADDR(.pcptext));LONG(SIZEOF(.pcptext));
    LONG(-1);                 LONG(-1);                  LONG(-1);
  } > CODE_MEM
  CORE_SEC(.sdata2)  : ALIGN(8) FLAGS(arsl)
  {
    PROVIDE(CORE_SYM(_SMALL_DATA2_) = 0x8000);
    *(.sdata.rodata)
    *(.sdata.rodata.*)
    *(.srodata)
    *(.srodata.*)
    *(.sdata2)
    *(.sdata2.*)
    *(.gnu.linkonce.sr.*)
  } > CODE_MEM
  /*
   * C++ exception handling tables.  NOTE: gcc emits .eh_frame
   * sections when compiling C sources with debugging enabled (-g).
   * If you can be sure that your final application consists
   * exclusively of C objects (i.e., no C++ objects), you may use
   * the -R option of the "strip" and "objcopy" utilities to remove
   * the .eh_frame section from the executable.
   */
  CORE_SEC(.eh_frame)  : ALIGN(8)
  {
    *(.gcc_except_table)
    __EH_FRAME_BEGIN__ = . ;
    KEEP (*(.eh_frame))
    __EH_FRAME_END__ = . ;
  } > CODE_MEM
  .jcr            : { KEEP (*(.jcr)) } > CODE_MEM
  .ctors          : FLAGS(al)
  {
    __CTOR_LIST__ = . ;
    LONG((__CTOR_END__ - __CTOR_LIST__) / 4 - 2);
    /* gcc uses crtbegin.o to find the start of
       the constructors, so we make sure it is
       first.  Because this is a wildcard, it
       doesn't matter if the user does not
       actually link against crtbegin.o; the
       linker won't look for a file to match a
       wildcard.  The wildcard also means that it
       doesn't matter which directory crtbegin.o
       is in.  */
    KEEP (*crtbegin.o(.ctors))
    /* We don't want to include the .ctor section from
       from the crtend.o file until after the sorted ctors.
       The .ctor section from the crtend file contains the
       end of ctors marker and it must be last */
    KEEP (*(EXCLUDE_FILE (*crtend.o ) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    LONG(0) ;
    __CTOR_END__ = . ;
        . = ALIGN(8);
  } > CODE_MEM
  .dtors          :  FLAGS(al)
  {
    __DTOR_LIST__ = . ;
    LONG((__DTOR_END__ - __DTOR_LIST__) / 4 - 2);
    KEEP (*crtbegin.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o ) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    LONG(0) ;
    __DTOR_END__ = . ;
        . = ALIGN(8);
  } > CODE_MEM
  /*
   * We're done now with the text part of the executable.  The
   * following sections are special in that their initial code or
   * data (if any) must also be stored in said text part of an
   * executable, but they "live" at completely different addresses
   * at runtime -- usually in RAM areas.  NOTE: This is not really
   * necessary if you use a special program loader (e.g., a debugger)
   * to load a complete executable consisting of code, data, BSS, etc.
   * into the RAM of some target hardware or a simulator, but it *is*
   * necessary if you want to burn your application into non-volatile
   * memories such as EPROM or FLASH.
   */
  .pcptext : ALIGN(8) FLAGS(axpl)
  {
    CORE_SYM(PCODE_BASE) = . ;
    KEEP(*(.pcptext))
    KEEP(*(.pcptext.*))
    . = ALIGN(8) ;
   CORE_SYM(PCODE_END) = . ;
  } > PCP_CODE AT> CODE_MEM
  .pcpdata : ALIGN(8) FLAGS(awpl)
  {
    CORE_SYM(PRAM_BASE) = . ;
    KEEP(*(.pcpdata))
    KEEP(*(.pcpdata.*))
    . = ALIGN(8) ;
    CORE_SYM(PRAM_END) = . ;
  } > PCP_DATA AT> CODE_MEM
  CORE_SEC(.data) : ALIGN(8) FLAGS(awl)
  {
    CORE_SYM(DATA_BASE) = . ;
    *(.data)
    *(.data.*)
    *(.gnu.linkonce.d.*)
    SORT(CONSTRUCTORS)
    CORE_SYM(DATA_END) = . ;
  } > DATA_MEM2 AT> LARGE_ROM
  CORE_SEC(.sdata)  : ALIGN(8) FLAGS(awsl)
  {
        CORE_SYM(SDATA_BASE) = . ;
    PROVIDE(CORE_SYM(__sdata_start) = .);
    PROVIDE(CORE_SYM(_SMALL_DATA_) = 0x8000);
    *(.sdata)
    *(.sdata.*)
    *(.gnu.linkonce.s.*)
  } > SDATA_MEM2  AT> CODE_MEM
  CORE_SEC(.sbss)  : ALIGN(8) FLAGS(aws)
  {
    PROVIDE(CORE_SYM(__sbss_start) = .);
    *(.sbss)
    *(.sbss.*)
    *(.gnu.linkonce.sb.*)
  } > SDATA_MEM2
  /*
   * Allocate space for BSS sections.
   */
  CORE_SEC(.bss)  : ALIGN(8) FLAGS(aw)
  {
    CORE_SYM(BSS_BASE) = . ;
    *(.bss)
    *(.bss.*)
    *(.gnu.linkonce.b.*)
    *(COMMON)
    . = ALIGN(8) ;
  } > BSS_MEM2
  CORE_SEC(.ustack) : ALIGN(8) FLAGS(aw)
  {
    CORE_SYM(__USTACK_TOP) =  . ;
    . +=  __USTACK__SIZE ;
    . = ALIGN(8) ;
    CORE_SYM(__USTACK) =  . ;
  } > BSS_MEM2
  CORE_SEC(.istack) : ALIGN(8) FLAGS(aw)
  {
    . +=  __ISTACK_SIZE ;
    . = ALIGN(8) ;
    CORE_SYM(__ISTACK) =  . ;
  } > BSS_MEM2
  CORE_SEC(.heap) : ALIGN(8) FLAGS(aw)
  {
    CORE_SYM(__HEAP) = . ;
    . +=  __HEAP__SIZE ;
    . = ALIGN(8) ;
    CORE_SYM(__HEAP_END) =  . ;
  } > BSS_MEM2
  /* define the CSA Memory area as an own section
   * this section will be allocated into the internal RAM
   * after the absolute addressable sections .zdata/.zbss
   * and allocate all internal memory not occupied by .zdata/.zbss
  */
  CORE_SEC(.csa) : ALIGN(64) FLAGS(aw)
  {
    CORE_SYM(__CSA_BEGIN) = . ;
    . +=  __CSA_SIZE ;
    CORE_SYM(__CSA_END) = .;
  } > CSA_MEM2
  CORE_SYM(__CSA_SIZE) = CORE_SYM(__CSA_END) - CORE_SYM(__CSA_BEGIN);
  CORE_SYM(_end) = CORE_SYM(__HEAP_END) ;
  PROVIDE(CORE_SYM(end) = CORE_SYM(_end)) ;
  /* Make sure CSA, stack and heap addresses are properly aligned.  */
  _. = ASSERT ((CORE_SYM(__CSA_BEGIN) & 0x3f) == 0 , "illegal CSA start address") ;
  _. = ASSERT ((CORE_SYM(__CSA_SIZE) & 0x3f) == 0 , "illegal CSA size") ;
  _. = ASSERT ((CORE_SYM(__ISTACK) & 7) == 0 , "ISTACK not doubleword aligned") ;
  _. = ASSERT ((CORE_SYM(__USTACK) & 7) == 0 , "USTACK not doubleword aligned") ;
  _. = ASSERT ((CORE_SYM(__HEAP_END) & 7) == 0 , "HEAP not doubleword aligned") ;
  /* Define a default symbol for address 0.  */
  NULL = DEFINED (NULL) ? NULL : 0 ;
  /*
   * DWARF debug sections.
   * Symbols in the DWARF debugging sections are relative to the
   * beginning of the section, so we begin them at 0.
   */
  /*
   * DWARF 1
   */
  .comment         0 : { *(.comment) }
  .debug           0 : { *(.debug) }
  .line            0 : { *(.line) }
  /*
   * GNU DWARF 1 extensions
   */
  .debug_srcinfo   0 : { *(.debug_srcinfo) }
  .debug_sfnames   0 : { *(.debug_sfnames) }
  /*
   * DWARF 1.1 and DWARF 2
   */
  .debug_aranges   0 : { *(.debug_aranges) }
  .debug_pubnames  0 : { *(.debug_pubnames) }
  /*
   * DWARF 2
   */
  .debug_info      0 : { *(.debug_info) }
  .debug_abbrev    0 : { *(.debug_abbrev) }
  .debug_line      0 : { *(.debug_line) }
  .debug_frame     0 : { *(.debug_frame) }
  .debug_str       0 : { *(.debug_str) }
  .debug_loc       0 : { *(.debug_loc) }
  .debug_macinfo   0 : { *(.debug_macinfo) }
  .debug_ranges    0 : { *(.debug_ranges) }
   /*
    * DWARF 2 control flow extension
    */
  .debug_control_flow 0 : { *(.debug_control_flow) }
  /*
   * SGI/MIPS DWARF 2 extensions
   */
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }
  /*
   * Optional sections that may only appear when relocating.
   */
  /*
   * Optional sections that may appear regardless of relocating.
   */
  .version_info    0 : { *(.version_info) }
  .boffs           0 : { KEEP (*(.boffs)) }
}
