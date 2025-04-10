proc run_verbosity_test {} {
  sc_fpga_ctrl_silence
  set MSG [capture {_SC_INTERNALS::sc_lib_print "silence!" }]

  if { $MSG ne "" } {
    error "the silence has been broken: $MSG"
  }

  sc_fpga_ctrl_verbose
  set MSG [capture {_SC_INTERNALS::sc_lib_print "verbosity!" }]

  if { $MSG ne "run_verbosity_test: verbosity!\n" } {
    error "deafening silence is unexpected... ($MSG)"
  }
  shutdown
}

