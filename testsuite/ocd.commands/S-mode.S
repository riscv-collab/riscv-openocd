.global _entry
.global main
.global end
.global check_s_mode
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

check_s_mode:
  la t2, loop
  csrw stvec, t2
end:
  ebreak

.balign DSTVEC_BALIGN
loop:
  j loop

.balign DMTVEC_BALIGN
change_mode:
  li    t0, 0x0800
  csrw  mstatus, t0
  la t1, check_s_mode
  csrw mepc, t1
  li t0, 0xFFFF 
  csrw medeleg, t0
  mret
