
.section .entry
.globl entry
.type entry,@function
entry:
    j 1f
    halt:
    ebreak
    1:
.option push
.option norelax
	la gp, __global_pointer$
.option pop
	la sp, _estack

    call start
    j halt
