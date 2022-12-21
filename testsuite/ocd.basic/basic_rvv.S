.altmacro

.global _entry
.global main
.global end
.global write_done
.global miserable_failure
.global ext_read_start
.global ext_read_done
.global read_vx0
.global pre_read_vx0

.macro gen_state_labels lbl_n
  .global read_vx\lbl_n
  .global pre_read_vx\lbl_n
  .global prepair_vx\lbl_n
.endm

.set i,0
.rept 32
   gen_state_labels %i
  .set i,i+1
.endr

.text
_entry:
  nop
main:
  li sp, (1 << 9) // mstatus.VS = 1 (initial)
  csrs  mstatus, sp

  // vtype: 11000'b { vlmul = 0, SEW = 64 }
  li s0, 0x18
  csrr    s1, vl
  vsetvl  zero, s1, s0

  csrr s1, vl
  srli s1, s1, 63
  bne zero, s1, miserable_failure

  // vl = 0x2
  li s0, 0x2
  csrr    s1, vtype
  vsetvl  zero, s0, s1

  csrr s1, vl
  srli s1, s1, 63

  bne zero, s1, miserable_failure

  li t2, 31

.macro slide regn
  li t1, 1
  slli t2, t1, \regn
  add s0, zero, t2
  sub s1, zero, t2
  vslide1down.vx v\regn, v\regn, s0
  vslide1down.vx v\regn, v\regn, s1
.endm

.set i,0
.rept 32
  slide %i
  .set i,i+1
.endr

write_done:
  j write_done
  nop
ext_read_start:
  nop

.macro read_vx regn
prepair_vx\regn:
  li t1, 2
  slli t2, t1, \regn
  add t3, zero, t2
  sub t4, zero, t2
  mv ra, zero
  mv sp, zero
pre_read_vx\regn:
  vmv.x.s ra, v\regn
  vslide1down.vx v\regn, v\regn, zero
  vmv.x.s sp, v\regn
read_vx\regn:
  bne ra, t3, miserable_failure
  bne sp, t4, miserable_failure
.endm
.set i,0
.rept 32
  read_vx %i
  .set i,i+1
.endr

ext_read_done:
  j ext_read_done
  nop
miserable_failure:
  j miserable_failure
