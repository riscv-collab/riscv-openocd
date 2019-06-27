#if XLEN == 64
# define LREG ld
# define SREG sd
# define REGBYTES 8
#else
# define LREG lw
# define SREG sw
# define REGBYTES 4
#endif

		.section .text.entry
		.global _start
_start:
		la 		sp, stack_end
		jal 	main
		ebreak

		.section .data
stack:
		.fill	40, REGBYTES, 0x8675309
stack_end:
