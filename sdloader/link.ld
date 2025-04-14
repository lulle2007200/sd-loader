ENTRY(_start)

SECTIONS {
	PROVIDE(__ipl_start = IPL_LOAD_ADDR);
	. = __ipl_start;
	.text_head : {
		*(.text._start);
		*(.text._irq_setup);
	}
	.text_loader : {
		/* loader must live in low iram (minimum lower than 0x40010000) */
		*loader.o(*);
	}
	.text_tail : {
		*(.text*);
	}
	.data : {
		*(.data*);
		*(.rodata*);
		. = ALIGN(0x10);
	}
	__ipl_end = .;
	.bss : {
		__bss_start = .;
		*(COMMON)
		*(.bss*)
		. = ALIGN(0x10);
		__bss_end = .;
	}
	__payload_size = __bss_end - __ipl_start;
}
