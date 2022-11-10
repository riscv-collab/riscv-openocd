set pre_tap_count 0

proc sc_pre_tap_hook {} {
  global pre_tap_count
  incr pre_tap_count
}

set target_conf_count 0
proc sc_target_configuration_hook {tgt} {
  global target_conf_count
  global NCORES
  if { $NCORES == 1 } {

    if { ![string equal $tgt riscv.cpu] } {
      echo "ERROR: unexpected target_name: $tgt"
      shutdown error
    }
  } else {
    if { ![string equal $tgt riscv.cpu$target_conf_count] } {
      echo "ERROR: unexpected target_name: $tgt / $target_conf_count"

      shutdown error
    }
  }
  incr target_conf_count
}

init

if { $target_conf_count != $NCORES } {
  echo "ERROR: unexpected number of calls to target_conf_hook: $target_conf_count"
  shutdown error
}

if { $pre_tap_count != 1 } {
  echo "ERROR: unexpected number of calls to pre_tap_hook: $pre_tap_count"
  shutdown error
}

shutdown
