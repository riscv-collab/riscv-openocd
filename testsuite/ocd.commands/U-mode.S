.global _entry
.global main
.global check_u_mode
.global change_mode
.global loop

.text
_entry:
  nop
main:
  la       t0, change_mode
  csrw     mtvec, t0
  ebreak

check_u_mode:
  ebreak

.balign 4
loop:
  j loop

.balign 4
change_mode:
  li    t0, 0x0000
  csrw  mstatus, t0
  la t1, check_u_mode
  csrw mepc, t1
  la t2, loop
  csrw mtvec, t2
  mret
  
