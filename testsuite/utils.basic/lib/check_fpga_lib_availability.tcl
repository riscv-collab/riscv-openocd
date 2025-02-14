proc check_fpga_lib_availability { expect_available } {
  set fpga_lib_functions [list \
    sc_fpga_ctrl_silence \
    sc_fpga_ctrl_verbose \
    sc_fpga_read_reg \
    sc_fpga_write_reg \
    sc_fpga_halt_all \
    sc_fpga_resume_all \
    sc_fpga_step_all \
    sc_fpga_zero_regs \
    sc_fpga_setup_semihosting \
    sc_fpga_find_target_by_hartid \
    sc_fpga_run_elf \
    sc_fpga_run_elf_smp \
    sc_fpga_run_bin \
    sc_fpga_run_bin_smp \
    sc_fpga_info \
  ]
  set loaded_fpga_functions [info procs sc_fpga_*]
  set not_processed_functions [list {*}$loaded_fpga_functions]

  echo "loaded_fpga_functions: $loaded_fpga_functions"
  foreach func $fpga_lib_functions {
    set found_idx [lsearch -exact $loaded_fpga_functions $func]
    if {$expect_available && ($found_idx == -1)} {
      echo "ERROR: $func DOES NOT EXISTS in the list of loaded funcitons"
      shutdown error
    }
    if {!$expect_available && ($found_idx >= 0)} {
      echo "ERROR: $func DOES EXISTS in the list of loaded funcitons"
      shutdown error
    }
    set processed_idx [lsearch -exact $not_processed_functions $func]
    set not_processed_functions [lreplace $not_processed_functions $processed_idx $processed_idx]
  }

  if {$expect_available && [llength $not_processed_functions] > 0 } {
    echo "ERROR: extra functions detected - $not_processed_functions"
    shutdown error
  }
}
