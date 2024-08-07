/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = 0x20020000;    /* end of RAM */
/* Generate a link error if heap and stack don't fit into RAM */
_Min_Stack_Size = 0x100; /* required amount of stack */

/* Specify the memory areas */
MEMORY
{
ALIAS_RAM(xrw)  : ORIGIN = 0x00000000, LENGTH = 112K
RAM (rw)        : ORIGIN = 0x20000000, LENGTH = 128K
FLASH (rx)      : ORIGIN = 0x8004000, LENGTH = 496K
}

/* Define output sections */
SECTIONS
{
  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >FLASH

  /* The program code and other data goes into FLASH */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH

  /* Constant data goes into FLASH */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(0x200);
	KEEP(*(.isr_vector_crash)) /* crash handler isr vector */
  } >FLASH

  .ARM : {
    __exidx_start = .;
    __exidx_end = .;
  } >FLASH   /* we provide our own exception handling stuff */

  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH
  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* setup VRAM/crashRAM area */
  .vram (NOLOAD) :
  AT (0x32000000) /* ugly hack to get openocd to ignore this section */
  {
	. = ALIGN(4);
	_svram = .;
	*(.vram .vram*)
	. = ALIGN(4);
	_evram = .;
	_ecstack = .;
  } >RAM

  .crash_bss (_svram) : 
  AT (0x34000000) /* ugly hack to get openocd to ignore this section */
  {
	*libstm_crash.a:*(.bss .bss* COMMON) /* crash bss */
	. = ALIGN(4);
	_ecbss = .;
  }

  _scidata = LOADADDR(.crash_data);

  .crash_data (_svram + SIZEOF(.crash_bss)) :
  {
	. = ALIGN(4);
	_scdata = .;
	*libstm_crash.a:*(.data .data*)
	_ecdata = .;
  } AT> FLASH

  /* used by the startup to initialize data */
  _sidata = LOADADDR(.data);
  _sridata = LOADADDR(.ram_func);

  /* Functions located in RAM are not allowed to call back into flash */
  .ram_func (_evram - ORIGIN(RAM)) :
  {
	  *(.ram_func .ram_func*)     /* ram located functions */
  } >ALIAS_RAM AT> FLASH

  _srdata = ADDR(.ram_func) + ORIGIN(RAM);
  _erdata = ADDR(.ram_func) + SIZEOF(.ram_func) + ORIGIN(RAM);

  .ram_isr_vector (ADDR(.ram_func) + SIZEOF(.ram_func)) (NOLOAD):
  {
	  *(.ram_isr_vector)
  } >ALIAS_RAM

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data (ADDR(.ram_isr_vector) + SIZEOF(.ram_isr_vector) + ORIGIN(RAM)) : 
  {
	ASSERT((ADDR(.ram_isr_vector) + SIZEOF(.ram_isr_vector) < 0x1c000), "ramfuncs stretch past the end of SRAM1");

    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */

    *(.data .data*)           /* .data sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >RAM AT> FLASH
  
  /* Uninitialized data section */
  . = ALIGN(4);
  .bss (NOLOAD) : AT (0x30000000) /* ugly hack to get openocd to ignore this section */
  {
    /* This is used by the startup in order to initialize the .bss secion */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss .bss* COMMON) /* ignore crash */

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
	end = .;

	/* ensure stack fits */
	. = . + _Min_Stack_Size;
  } >RAM

  /* debug data region; filled out by build scripts manually with objcopy */
  .fw_dbg :
  {
	  . = ALIGN(4);
	  _dbg_data_begin = .;
	  . = . + 4;
  } >FLASH
  
  /DISCARD/ :
  {
	*(.ARM.ex*)
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}

NOCROSSREFS_TO(.text .ram_func)
NOCROSSREFS_TO(.rodata .ram_func)

