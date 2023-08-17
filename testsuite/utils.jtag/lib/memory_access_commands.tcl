set commands [list \
    "ocdjtag_riscv_reset_halt_hart" "0" \
    "ocdjtag_riscv_halt_hart" "0" \
    "ocdjtag_riscv_store_memory" "0 $ENTRY_ADDR 32 $true_data"]
