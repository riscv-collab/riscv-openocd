proc ocd_test_target_configuration_hook {} {
  foreach t [target names] {
    echo "running configuration hook for $t"
  }
  global GLOBAL_VARIABLE_IN_HOOK
  set GLOBAL_VARIABLE_IN_HOOK "GLOBAL_VARIABLE_IN_HOOK IS SET!"
}

