ENTRY(_start)

SECTIONS {
	PROVIDE(__loader_start = LOADER_LOAD_ADDR);
	. = __loader_start;
	.text : {
		*(.text._start);
		*(.text*);
		*(.data*);

		/* Force bss and zerofill into text so they are marked as progbits aswell */
		*(.rodata*);
		*(.bss*);
		*(.COMMON*);
		. = ALIGN(0x10);
	}
	__loader_end = .;

	/* before loader is encrypted, 0x20 zero bytes must be appended */
	__encrypt_start = __loader_end + 0x20;
	__payload_start = __encrypt_start;
	
	__encrypt_end   = __loader_start + 0x10000;
	__payload_end   = __encrypt_end;
}
