/* === entry point ============================== */
ENTRY(Reset_Handler)

/* === stack and heap definitions =============== */
_estack = 0x10002000;	/* end of "RAM" Ram type memory */

_Min_Heap_Size = 0x100;	/* required amount of heap  */
_Min_Stack_Size = 0x100;	/* required amount of stack */


/* === memory layout ============================ */
MEMORY
{
    flash (rx)  : org = 0x00000000, len = 32k
    ram   (rwx) : org = 0x10000000, len = 8k
}
_flash_start = ORIGIN(flash);
_flash_end = ORIGIN(flash)+LENGTH(flash);

/* === linker sections ========================= */
SECTIONS
{
	/* --- place vic vector table - into flash --- */
	.isr_vector :
	{
		. = ALIGN(4);
		KEEP (*(.isr_vector))
		. = ALIGN(4);
	} >flash

	/* ---- place program code - into flash --- */
	.text :
    {
		. = ALIGN(4);
		KEEP (*(.after_vectors))	/* isr support code */
		*(.text)					/* code section */
		*(.text*)
		*(.glue_7)					/* glue arm to thumb code */
		*(.glue_7t)	 				/* glue thumb to arm code */
		*(.eh_frame)
		
		KEEP(*(.init))
		KEEP(*(.fini))

		. = ALIGN(4);
        _etext = .;
	} >flash        
        
	/* --- constant data - into flash --- */	
	.rodata :
	{
		. = ALIGN(4);
		*(.rodata)
		*(.rodata*)
		. = ALIGN(4);
	} >flash

	.ARM.extab :
	{
		. = ALIGN(4);
		*(.ARM.extab* .gnu.linkonce.armextab.*)
		. = ALIGN(4);
	} >flash

	.ARM :
	{
		. = ALIGN(4);
		__exidx_start = .;
		*(.ARM.exidx*)
		__exidx_end = .;
		. = ALIGN(4);
	} >flash

	.preinit_array :
	{
		. = ALIGN(4);
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP (*(.preinit_array*))
		PROVIDE_HIDDEN (__preinit_array_end = .);
		. = ALIGN(4);
	} >flash
  
  	.init_array :
  	{
		. = ALIGN(4);
    	PROVIDE_HIDDEN (__init_array_start = .);
    	KEEP (*(SORT(.init_array.*)))
    	KEEP (*(.init_array*))
    	PROVIDE_HIDDEN (__init_array_end = .);
		. = ALIGN(4);
  	} >flash
  
  	.fini_array :
  	{
		. = ALIGN(4);
    	PROVIDE_HIDDEN (__fini_array_start = .);
    	KEEP (*(SORT(.fini_array.*)))
    	KEEP (*(.fini_array*))
    	PROVIDE_HIDDEN (__fini_array_end = .);
		. = ALIGN(4);
  	} >flash

	/* --- data sections - into ram --- */
	_sidata = LOADADDR(.data);

    .data : 
    {
		. = ALIGN(4);
	  	_sdata = .;
	    *(.data)
		*(.data*)
		. = ALIGN(8);
	  	_edata = .;
    } >ram AT >flash

	. = ALIGN(4);
	.bss : 
	{
		_sbss = .;
		__bss_start__ = _sbss;
	   	*(.bss);
		*(.bss*)
		*(COMMON)

		. = ALIGN(8);
	   	_ebss = .;	  
	   	__bss_end__ = _ebss;
	} >ram

	/* --- User_heap_stack section, used to check that there is enough ram left */
  	._user_heap_stack :
	{
    	. = ALIGN(8);
    	PROVIDE ( end = . );
    	PROVIDE ( _end = . );
    	. = . + _Min_Heap_Size;
    	. = . + _Min_Stack_Size;
    	. = ALIGN(8);
  	} >ram

	/* Remove information from the compiler libraries */
	/DISCARD/ :
	{
		libc.a ( * )
		libm.a ( * )
		libgcc.a ( * )
	}

 	.ARM.attributes 0 : { *(.ARM.attributes) }
}
