# this hook is needed mostly for targets that have only EQ trigger implemented
# (like SCR1). Otherwise wp_workaround_test will fail because it won't be
# able to set any watchpoints since universal launcher disables them
proc sc_pre_tap_hook {} {
  sc_target_config disable_EQ_triggers 0
}
