.global _entry
.global main
.global end
.global load_from
.global store_to
.global load_instr
.global store_instr

.text
_entry:
  nop
main:
  la x1, load_from
  la x2, store_to
load_instr:
  lw x3, 0(x1)
store_instr:
  sw x3, 0(x2)
end:
  j main

.balign 8
load_from:
.8byte 0x0
store_to:
.8byte 0x0
