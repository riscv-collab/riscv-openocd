set NCORES $::env(NCORES)
set IS_SIM $::env(IS_SIM)

sc_target_config harts_num $NCORES
if { $IS_SIM == 1 } {
  sc_target_config new_tap_for_each_target 0
}
