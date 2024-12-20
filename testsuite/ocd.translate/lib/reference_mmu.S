.global _entry
.global _mode_set
.global _after_sfence
.global _load_instr
.global _data_va_address
.global _data_pa_address
.global _mmu_table_address
.global _pmpcfg_failure
.global _mstatus_failure
.global _satp_failure
.global _data_end

.text
_entry:
  xor ra, ra, ra
_mode_set:
  // read va
  la    s1, _data_va_address
  la    s2, _data_pa_address
  la    s3, _mmu_table_address

#if __riscv_xlen == 32
  lw    s1, 0(s1)
  lw    s2, 0(s2)
  lw    s3, 0(s3)
#else
  ld    s1, 0(s1)
  ld    s2, 0(s2)
  ld    s3, 0(s3)
#endif

  li s4, -1
  srli s4, s4, 8
  csrw pmpaddr0, s4
  // pmpcfg.entry.bit0 - Read access
  // pmpcfg.entry.bit1 - Write access
  // pmpcfg.entry.bit2 - Execute access
  // pmpcfg.entry.bit3-4 - Address match mode (TOR, A=1 ; NAPOT, A=3)
  li s4, 0xf
  csrw pmpcfg0, s4
  csrr s9, pmpcfg0
  andi s9, s9, 0x1f
  bne  s9, s4, _pmpcfg_failure

#if __riscv_xlen == 32
  // for rv32 we unconditionally set satp.MODE = 1
  li ra, 1
  slli  s5, ra, 31
#else
  // ra should contain the desired mode after the first instruction
  // s5 contains satp.MODE now
  slli  s5, ra, 60
#endif
  // mstatus.MPRV:{17} = 1 and mstatus.SUM = 1
  li    ra, (1 << 17) | (1 << 18)
  // mstatus.MPP:{12-11}
  li    s6, (3 << 11) // mask
  // desired mstatus.MPP
  li    s7, (1 << 11)
  csrr  s8, mstatus
  or    s8, s8, s6
  // zero-out mstatus.MPP
  xor   s8, s8, s6
  // set mstatus.MPP = 1
  or    s8, s8, s7
  // set mstatus.MPRV and mstatus.SUM
  or    s8, s8, ra

  csrw  mstatus, s8
  csrr s9, mstatus
  bne s8, s9, _mstatus_failure

  // initialize SATP value (PPN address)
  srli  s8, s3, 12
  // set satp.MODE
  or    s8, s8, s5
  csrw satp, s8
  csrr s9, satp
  bne s8, s9, _satp_failure
  sfence.vma
_load_instr:
  lw ra, 0(s1)
_the_loop:
  j _the_loop
_pmpcfg_failure:
  j _pmpcfg_failure
_mstatus_failure:
  j _mstatus_failure
_satp_failure:
  j _satp_failure

.balign 8
#if __riscv_xlen == 32
_data_va_address:
.4byte 0
_data_pa_address:
.4byte 0
_mmu_table_address:
.4byte 0
#else
_data_va_address:
.8byte 0
_data_pa_address:
.8byte 0
_mmu_table_address:
.8byte 0
#endif
_data_end:
.8byte 0
