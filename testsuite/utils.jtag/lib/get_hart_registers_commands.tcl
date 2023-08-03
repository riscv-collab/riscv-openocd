set commands [list \
    "ocdjtag_riscv_reset_halt_hart" "0" \
    "ocdjtag_riscv_halt_hart" "0" \
    "ocdjtag_riscv_store_register" "0 $s0_reg $true_data" \
    "ocdjtag_riscv_load_register" "0 $s0_reg"]
