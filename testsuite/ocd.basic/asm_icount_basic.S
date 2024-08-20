
.global _entry
.global startup_end

.global code_payload
.global code_payload_inst1
.global code_payload_inst5
.global code_payload_sup

.global stvect_start
.global mtvect_start

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
  li       t0, 1

  la       a0, mtvect_start
  csrw     mtvec, a0

  // this will be set by external debugger
  // la       a0, stvect_start
  // csrw     stvec, a0

  j startup_end

startup_end:
  j startup_end

code_payload:
  addi t0, t0, 1
code_payload_inst1:
  addi t0, t0, 2
  addi t0, t0, 3
  addi t0, t0, 4
  addi t0, t0, 5
code_payload_inst5:
  addi t0, t0, 6
  addi t0, t0, 7
code_payload_sup:
  j code_payload

.balign DSTVEC_BALIGN
stvect_start:
1:
  j 1b

.balign DMTVEC_BALIGN
mtvect_start:
1:
  j 1b
