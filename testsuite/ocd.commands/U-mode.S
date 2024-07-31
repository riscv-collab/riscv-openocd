.global _entry
.global main
.global check_u_mode
.global change_mode
.global loop

.text
_entry:
  li t0, -1
#if __riscv_xlen >= 64
  srli t0, t0, 8
#endif
  csrw pmpaddr0, t0
  // pmpcfg.entry.bit0 - Read access
  // pmpcfg.entry.bit1 - Write access
  // pmpcfg.entry.bit2 - Execute access
  // pmpcfg.entry.bit3-4 - Address match mode (TOR, A=1 ; NAPOT, A=3)
  li t0, 0x2f
  csrw pmpcfg0, t0
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
