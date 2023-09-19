proc ocd_test_target_configuration_hook {} {
  global GLOBAL_VARIABLE_IN_HOOK
  set GLOBAL_VARIABLE_IN_HOOK 0

  foreach t [target names] {

    $t configure -event examine-end {
      global GLOBAL_VARIABLE_IN_HOOK
      set GLOBAL_VARIABLE_IN_HOOK [expr {$GLOBAL_VARIABLE_IN_HOOK + 1}]
    }
  }
}
