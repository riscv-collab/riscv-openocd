OUTPUT_ARCH( "riscv" )

/* Entry Point */
ENTRY( entry )

/* System memory map */
MEMORY
{
	/* Application is stored in and executes from SRAM */
	RAM (RWX) : ORIGIN = 0x20000000, LENGTH = 10K
}

/* Section allocation in memory */
SECTIONS
{
	__stack_size = 0x100;
	.text :
	{
		_text = .;
		*(.entry*)
		*(.text*)
		_etext = .;
	} > RAM

	.data :
	{	_data = .;
		PROVIDE( __global_pointer$ = . + 0x400 );
		*(.rodata*)
		*(.data*)
		_edata = .;
	}

	.bss :
	{
		__bss_start__ = .;
		_bss = .;
		*(.bss*)
		*(COMMON)
		_ebss = .;
		__bss_end__ = .;
	} > RAM

	.stack ORIGIN(RAM) + LENGTH(RAM) - __stack_size : 
	{
		PROVIDE( _stack = . );
		. = __stack_size;
		PROVIDE( _estack = . );
	} > RAM
}
