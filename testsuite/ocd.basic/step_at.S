.global _entry
.global main
.global end
.global instr1
.global instr2
.global instr3
.global after3

.text
_entry:
  nop
  li x5, 42
  li x6, 666
  li x7, 777
main:
  nop
end:
  j main

.balign 16
instr1:
  add x1, x5, x6
  add x1, x1, x6

.balign 16
instr2:
  sub x2, x5, x6
  add x2, x2, x7

.balign 16
instr3:
  add x3, x6, x7
after3:
  add x3, x3, x7
