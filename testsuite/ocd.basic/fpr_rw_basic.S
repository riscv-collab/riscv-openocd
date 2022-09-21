.global _entry
.global end

.text
_entry:
  nop

 csrr  x1, misa
 andi  x1, x1, (1 << ('F' - 'A'))
 beqz  x1, miserable_failure
 li    x1, (1 << 13) // mstatus.FS = 1 (initial)
 csrs  mstatus, x1
 csrw  fcsr, zero

 li x1, 1
 fcvt.s.w ft0,  x1

 addi x1, x1, 1
 fcvt.s.w ft1,  x1

 addi x1, x1, 1
 fcvt.s.w ft2,  x1

 addi x1, x1, 1
 fcvt.s.w ft3,  x1

 addi x1, x1, 1
 fcvt.s.w ft4,  x1

 addi x1, x1, 1
 fcvt.s.w ft5,  x1

 addi x1, x1, 1
 fcvt.s.w ft6,  x1

 addi x1, x1, 1
 fcvt.s.w ft7,  x1

 addi x1, x1, 1
 fcvt.s.w fs0,  x1

 addi x1, x1, 1
 fcvt.s.w fs1,  x1

 addi x1, x1, 1
 fcvt.s.w fa0,  x1

 addi x1, x1, 1
 fcvt.s.w fa1,  x1

 addi x1, x1, 1
 fcvt.s.w fa2,  x1

 addi x1, x1, 1
 fcvt.s.w fa3,  x1

 addi x1, x1, 1
 fcvt.s.w fa4,  x1

 addi x1, x1, 1
 fcvt.s.w fa5,  x1

 addi x1, x1, 1
 fcvt.s.w fa6,  x1

 addi x1, x1, 1
 fcvt.s.w fa7,  x1

 addi x1, x1, 1
 fcvt.s.w fs2,  x1

 addi x1, x1, 1
 fcvt.s.w fs3,  x1

 addi x1, x1, 1
 fcvt.s.w fs4,  x1

 addi x1, x1, 1
 fcvt.s.w fs5,  x1

 addi x1, x1, 1
 fcvt.s.w fs6,  x1

 addi x1, x1, 1
 fcvt.s.w fs7,  x1

 addi x1, x1, 1
 fcvt.s.w fs8,  x1

 addi x1, x1, 1
 fcvt.s.w fs9,  x1

 addi x1, x1, 1
 fcvt.s.w fs10, x1

 addi x1, x1, 1
 fcvt.s.w fs11, x1

 addi x1, x1, 1
 fcvt.s.w ft8,  x1

 addi x1, x1, 1
 fcvt.s.w ft9,  x1

 addi x1, x1, 1
 fcvt.s.w ft10, x1

 addi x1, x1, 1
 fcvt.s.w ft11, x1

end:
 j end
miserable_failure:
 j miserable_failure
