proc number_of_pending_wp {} {
  return [llength [split [wp] "\n"]]
}

proc wp_workaround_test_basic { wp_num } {
  reset halt
  # here test assumes that target is capable to set at least 2 watchpoints
  echo "set targets watchpoints"
  set WP_ADDR 0
  set WP_LEN 1
  set WATCHPOINTS [list]

  for {set i 0} {$i < $wp_num} {incr i} {
    lappend WATCHPOINTS [list [expr {$WP_ADDR + $i}] [expr {$WP_LEN +$i}]]
  }
  foreach wp_item $WATCHPOINTS {
    wp [lindex $wp_item 0] [lindex $wp_item 1]
  }
  set WP_NUM [llength $WATCHPOINTS]

  set EXPECTED_WP_LISTING [wp]

  if {[number_of_pending_wp] != $WP_NUM} {
    error "unexpected number of watchpoints set"
  }
  sc_lib_watchpoints_stash
  if {[number_of_pending_wp] != 0} {
    error "watchoint are pending after the call to stash"
  }

  wp 0 4
  if {![catch {sc_lib_watchpoints_restore} err]} {
    error "sc_lib_watchpoints_restore unexpectedly succeeded while wp pending"
  }
  rwp 0

  sc_lib_watchpoints_restore
  if {[number_of_pending_wp] != $WP_NUM} {
    error "unexpected number of watchpoitns after restore"
  }

  if {$EXPECTED_WP_LISTING != [wp]} {
    error "unexpected wp state after restore"
  }

  sc_lib_watchpoints_stash
  if {[number_of_pending_wp] != 0} {
    error "watchoint are pending after the second stash"
  }
  sc_lib_watchpoints_stash_drop
  if {[number_of_pending_wp] != 0} {
    error "watchoint are pending after the stash drop"
  }
  if {[catch {sc_lib_watchpoints_restore} err]} {
    error "sc_lib_watchpoints_stash unexpectedly failed while stash empty"
  }
}

proc wp_workaround_test_running_target { Elf Entry } {
  reset halt
  load_image $Elf
  resume $Entry
  if {![catch {sc_lib_watchpoints_stash} err]} {
    error "sc_lib_watchpoints_stash unexpectedly succeeded on a running target"
  }
  if {![catch {sc_lib_watchpoints_restore} err]} {
    error "sc_lib_watchpoints_restore unexpectedly succeeded on a running target"
  }
}

proc wp_workaround_test {ElfFile EntryPoint} {
  if {[catch {
    wp_workaround_test_basic 2
    wp_workaround_test_basic 1
    wp_workaround_test_running_target $ElfFile $EntryPoint
  } err]} {
    echo "fatal error: $err"
    shutdown error
  }
  shutdown
}

