.global _entry
.global main
.global end
.global work_area_start
.global work_area_end
.global mdd_test_data
.global loop

.text
_entry:
  nop
main:
#if __riscv_xlen >= 64
  addi    x1, x0, 2
#else
  addi    x1, x0, 1
#endif
  sll    x2, x1, x1
  sll    x3, x2, x1
  sll    x4, x3, x1
  sll    x5, x4, x1
  sll    x6, x5, x1
  sll    x7, x6, x1
  sll    x8, x7, x1
  sll    x9, x8, x1
  sll    x10, x9, x1
  sll    x11, x10, x1
  sll    x12, x11, x1
  sll    x13, x12, x1
  sll    x14, x13, x1
  sll    x15, x14, x1
  sll    x16, x15, x1
  sll    x17, x16, x1
  sll    x18, x17, x1
  sll    x19, x18, x1
  sll    x20, x19, x1
  sll    x21, x20, x1
  sll    x22, x21, x1
  sll    x23, x22, x1
  sll    x24, x23, x1
  sll    x25, x24, x1
  sll    x26, x25, x1
  sll    x27, x26, x1
  sll    x28, x27, x1
  sll    x29, x28, x1
  sll    x30, x29, x1
  sll    x31, x30, x1
end:
  j main

loop:
  j loop

.balign 16
work_area_start:
  .8byte 0xdeadbabe00baddad
  .8byte 0xfeedbabedadc0def
  .fill 2044, 4, 0xdeadbabe
work_area_end:
mdd_test_data:
  .8byte 0xdeadbabe00baddad
  .8byte 0x1020304050607080
  .8byte 0x90a0b0c0d0e0f010
