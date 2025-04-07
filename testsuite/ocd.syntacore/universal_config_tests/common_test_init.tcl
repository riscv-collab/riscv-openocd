set NCORES $::env(NCORES)
set IS_SIM $::env(IS_SIM)

proc unicfg_testlib_create_common_targets {} {

  global NCORES
  global IS_SIM

  if { $IS_SIM == 1 } {
    sc_target_config jtag_topology ${NCORES}riscvl5
  } else {
    sc_target_config harts_num $NCORES
  }
}
