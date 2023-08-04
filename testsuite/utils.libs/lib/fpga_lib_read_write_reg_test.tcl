proc reg_value {raw_value} {
  return [string trim [lindex [split $raw_value :] 1]]
}

proc run_test { elf entry_point xlen} {
  set entry_point [expr $entry_point]
  sc_fpga_run_elf_smp $elf $entry_point
  sc_fpga_halt_all

  echo "[reg pc $entry_point]"

  set expected_fp 4294967295
  if {$xlen == 64} {
    set expected_fp 18446744073709551615
  }
  echo "[reg fp $expected_fp]"

  set current_pc [reg_value [reg pc]]
  set current_misa [reg_value [reg misa]]

  set pc_from_lib [sc_fpga_read_reg pc]
  set misa_from_lib [sc_fpga_read_reg misa]
  set fp_from_lib [sc_fpga_read_reg fp]

  if {$current_pc != $entry_point} {
    error "current_pc is $current_pc, while expecting $entry_point"
  }
  if {$current_pc != $pc_from_lib} {
    error "current_pc is $current_pc, while lib returned $pc_from_lib"
  }
  if {$current_misa != $misa_from_lib} {
    error "current_pc is $current_misa, while lib returned $misa_from_lib"
  }
  echo "fp_from_lib: $fp_from_lib"
  if {$fp_from_lib ne $expected_fp} {
    error "expected_fp is $expected_fp, while lib returned $fp_from_lib"
  }
}

proc run_read_write_reg_test { elf entry_point xlen} {
  if {[catch { run_test $elf $entry_point $xlen}]} {
    shutdown error
  }
  echo "Great Success"
  shutdown
}
