.global _entry
.global main
.global end
.global check_s_mode
.global change_mode
.global loop

.text
_entry:
  nop
main:
  la       t0, change_mode
  csrw     mtvec, t0
  ebreak

check_s_mode:
  la t2, loop
  csrw stvec, t2
end:
  ebreak

.balign 4
loop:
  j loop

.balign 4
change_mode:
  li    t0, 0x0800
  csrw  mstatus, t0
  la t1, check_s_mode
  csrw mepc, t1
  li t0, 0xFFFF 
  csrw medeleg, t0
  mret
  
