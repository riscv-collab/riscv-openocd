.global _entry
.global main
.global work_area_start
.global load_instr
.global simple_instr

.option norvc

.text
_entry:
  nop
main:
  li x1, 1
  la x2, work_area_start
load_instr:
  lw x3, 0(x2)
  nop
  nop
simple_instr:
  nop
  j main

.balign 16
work_area_start:
  .8byte 0xdeadbabe00baddad
