proc run_test { elf entry_point } {
  set entry_point [expr $entry_point]
  sc_fpga_run_elf_smp $elf $entry_point
  sc_fpga_halt_all
  sc_fpga_step_all $entry_point
  foreach t [target names] {
    targets $t
    set cur_pc [sc_fpga_read_reg pc]
    set expected_pc [expr {$entry_point + 4}]
    if { $cur_pc != $expected_pc } {
      error "unexpected pc $cur_pc on $t, expecting $expected_pc"
    }
  }
  sc_fpga_step_all
  foreach t [target names] {
    targets $t
    set cur_pc [sc_fpga_read_reg pc]
    set expected_pc [expr {$entry_point + 8}]
    if { $cur_pc != $expected_pc } {
      error "unexpected pc $cur_pc on $t, expecting $expected_pc"
    }
  }
}
proc run_step_all_test { elf entry_point } {
  if {[catch { run_test $elf $entry_point }]} {
    shutdown error
  }
  shutdown
}
