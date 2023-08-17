set ft0_reg 0x1020
set s0_reg 0x1008

ocdjtag_riscv_reset_halt_hart 0

if {![catch {ocdjtag_riscv_load_register 0 $ft0_reg} var]} {
    echo "ERROR: unexpected success"
}

ocdjtag_riscv_load_register 0 $s0_reg

ocdjtag_riscv_step_hart 0

if {![catch {ocdjtag_riscv_load_register 0 $ft0_reg} var]} {
    echo "ERROR: unexpected success"
}

ocdjtag_riscv_load_register 0 $s0_reg