proc readRegister { NameOrNum } {
  return [expr [string trim [lindex [split [reg $NameOrNum] :] 1]]]
}

proc safeReset {} {
  reset halt
  # set PCAtReset [readRegister pc]
  # this is a 4-byte branch to self
  # mww $PCAtReset 0x0000006f
  reset
}
proc testHartIdSearch { } {
  set HARTS_NUM [llength [target names]]
  for { set HartIdx 0 } { $HartIdx < $HARTS_NUM } { incr HartIdx } {
    sc_fpga_halt_all
    set CurTarget [target current]
    set Target [sc_fpga_find_target_by_hartid $HartIdx]
    if {$CurTarget ne [target current]} {
      error "unexpected switch of active target from $CurTarget to [target current]"
    }
    targets $Target
    if {[readRegister mhartid] != $HartIdx } {
      error "unexpected hart id $HartID, expected $HartIdx, target: $Target"
    }
  }
  echo "HartID search validated"
}

proc testAllTargetsHalted {} {
  foreach t [target names] {
    if {[$t curstate] != "halted"} {
      error "sc_fpga_halt_all should halt all targets, but $t is [$t curstate]"
    }
  }
  echo "all targets are halted"
}

proc testAllTargetsRunning {} {
  foreach t [target names] {
    if {[$t curstate] != "running"} {
      error "sc_fpga_resume_all should resume all targets, but $t is [$t curstate]"
    }
  }
  echo "all targets are running"
}

proc testCurrentTargetRunning {} {
  set CurrentTarget [target current]
  foreach t [target names] {
    set CurState [$t curstate]
    if {$CurrentTarget == $t} {
      if {$CurState != "running"} {
        echo "[targets]"
        error "current target should be running , but $t is in $CurState"
      }
    } else {
      # now this depends on an smp mode. In case of non-smp other targets
      # should be halted
      if {[string trim [smp]] == "off"} {
        if {$CurState != "halted"} {
          echo "[targets]"
          error "$t should be halted , but $t is in $CurState"
        }
      } else {
        if {$CurState != "running"} {
          echo "[targets]"
          error "$t should be running , but $t is in $CurState"
        }
      }
    }
  }
}


proc testZeroOutRegs {} {
  set ValueCounter 1
  foreach t [target names] {
    for { set RegIdx 1 } { $RegIdx < 32 } { incr RegIdx } {
      targets $t
      reg $RegIdx $ValueCounter
      incr ValueCounter
    }
  }
  sc_fpga_zero_regs
  foreach t [target names] {
    for { set RegIdx 0 } { $RegIdx < 32 } { incr RegIdx } {
      targets $t
      set Value [readRegister $RegIdx]
      if {$Value != 0} {
        error "testZeroOutRegs test failed for GPR($RegIdx) = $Value at $t"
      }
    }
  }
  echo "testZeroOutRegs passed"
}

proc run_basic_tests {} {
  echo "=== SC FPGA INFO ===\n[sc_fpga_info]\n=== ------------ ==="
  set EntryAddr 0

  sc_fpga_halt_all
  testAllTargetsHalted
  sc_fpga_resume_all
  testAllTargetsRunning
  if {![catch {sc_fpga_resume_all} r]} {
    error "sc_fpga_resume_all should report an error when targets already running"
  }
  echo "sc_fpga_resume_all reported expected error: $r"
  if {![catch {sc_fpga_resume_all $EntryAddr} r]} {
    error "sc_fpga_resume_all should report an error when targets already running"
  }
  echo "sc_fpga_resume_all reported expected error: $r"
  testHartIdSearch
  testZeroOutRegs

  safeReset
  testAllTargetsRunning
}

proc run_test_check_state { expected_target expected_pc } {
  sc_fpga_halt_all

  if {[target current] ne $expected_target} {
    echo "[targets]"
    error "unexpected current target: [target current], expecting $expected_target"
  }
  set current_pc [readRegister pc]
  if {$current_pc != $expected_pc} {
    echo "[targets]"
    error "expected pc: $expected_pc, actual: $current_pc"
  }
}

proc run_test_adjust_active_target {} {
  targets [lindex [target names] 0]
}

proc test_execution_log { log status_code expected_result expected_line } {
  if { $expected_result == "pass" } {
    if {$status_code != 0} {
      error "unexpected status code $status_code while expecting success(0)"
    }
  } else {
    if {$status_code == 0} {
      error "unexpected status code $status_code while expecting success(!0)"
    }
  }

  set LastLine [lindex [split [string trim $log] "\n"] end]
  if {[string first $expected_line $LastLine] == -1} {
    error "\n--$expected_line\nis not found in\n--$LastLine"
  }
}

proc run_binary_test { bin_file load_address entry_point target_name } {
  set expected_target $target_name
  set default_target [lindex [target names] 0]
  if {$target_name eq ""} {
    set expected_target $default_target
  }

  sc_fpga_run_bin_smp $bin_file $load_address
  testAllTargetsRunning
  run_test_check_state $default_target $load_address

  run_test_adjust_active_target
  sc_fpga_run_bin_smp $bin_file $load_address ""
  testAllTargetsRunning
  run_test_check_state $default_target $load_address

  run_test_adjust_active_target
  sc_fpga_run_bin_smp $bin_file $load_address "" $target_name
  testAllTargetsRunning
  run_test_check_state $expected_target $load_address

  run_test_adjust_active_target
  sc_fpga_run_bin_smp $bin_file $load_address $entry_point $target_name
  testAllTargetsRunning
  run_test_check_state $expected_target $entry_point

  set CapturedExecution [capture \
    { set CatchResult [ catch { \
      sc_fpga_run_bin_smp /tmp/bla/bleh/bla $load_address $entry_point $target_name} Dummy ] } ]
  test_execution_log $CapturedExecution $CatchResult fail \
    "couldn't open /tmp/bla/bleh/bla"

  set CapturedExecution [capture \
    { set CatchResult [ catch { \
      sc_fpga_run_bin_smp $bin_file $load_address $entry_point $target_name} Dummy ] } ]
  test_execution_log $CapturedExecution $CatchResult pass \
    "started execution of $bin_file with $entry_point as entry point"

  run_test_adjust_active_target
  sc_fpga_run_bin $bin_file $load_address
  testCurrentTargetRunning
  run_test_check_state $default_target $load_address

  run_test_adjust_active_target
  sc_fpga_run_bin $bin_file $load_address ""
  testCurrentTargetRunning
  run_test_check_state $default_target $load_address

  run_test_adjust_active_target
  sc_fpga_run_bin $bin_file $load_address "" $target_name
  testCurrentTargetRunning
  run_test_check_state $expected_target $load_address

  run_test_adjust_active_target
  sc_fpga_run_bin $bin_file $load_address $entry_point $target_name
  testCurrentTargetRunning
  run_test_check_state $expected_target $entry_point

  # test
  set CapturedExecution [capture \
    { set CatchResult [ catch { \
      sc_fpga_run_bin /tmp/bla/bleh/bla $load_address $entry_point $target_name} Dummy ] } ]
  test_execution_log $CapturedExecution $CatchResult fail \
    "couldn't open /tmp/bla/bleh/bla"

  set CapturedExecution [capture \
    { set CatchResult [ catch { \
      sc_fpga_run_bin $bin_file $load_address $entry_point $target_name} Dummy ] } ]
  test_execution_log $CapturedExecution $CatchResult pass \
    "started execution of $bin_file with $entry_point as entry point"
}

proc run_elf_test { elf_file entry_point target_name } {
  set expected_target $target_name
  set default_target [lindex [target names] 0]
  if {$target_name eq ""} {
    set expected_target $default_target
  }

  sc_fpga_run_elf_smp $elf_file $entry_point
  testAllTargetsRunning
  run_test_check_state $default_target $entry_point

  sc_fpga_run_elf_smp $elf_file $entry_point $target_name
  testAllTargetsRunning
  run_test_check_state $expected_target $entry_point

  set CapturedExecution [capture \
    { set CatchResult [ catch { \
      sc_fpga_run_elf_smp /tmp/bla/bleh/bla $entry_point $target_name} Dummy ] } ]
  test_execution_log $CapturedExecution $CatchResult fail \
    "couldn't open /tmp/bla/bleh/bla"

  set CapturedExecution [capture \
    { set CatchResult [ catch { \
      sc_fpga_run_elf_smp $elf_file $entry_point $target_name} Dummy ] } ]
  test_execution_log $CapturedExecution $CatchResult pass \
    "started execution of $elf_file with $entry_point as entry point"

  sc_fpga_run_elf $elf_file $entry_point
  testCurrentTargetRunning
  run_test_check_state $default_target $entry_point

  sc_fpga_run_elf $elf_file $entry_point $target_name
  testCurrentTargetRunning
  run_test_check_state $expected_target $entry_point

  set CapturedExecution [capture \
    { set CatchResult [ catch { \
      sc_fpga_run_elf /tmp/bla/bleh/bla $entry_point $target_name} Dummy ] } ]
  test_execution_log $CapturedExecution $CatchResult fail \
    "couldn't open /tmp/bla/bleh/bla"

  set CapturedExecution [capture \
    { set CatchResult [ catch { \
      sc_fpga_run_elf $elf_file $entry_point $target_name} Dummy ] } ]
  test_execution_log $CapturedExecution $CatchResult pass \
    "started execution of $elf_file with $entry_point as entry point"
}

proc run_binary_tests { bin_file load_address entry_point } {
  safeReset
  testAllTargetsRunning

  run_binary_test $bin_file $load_address $entry_point ""
  foreach t [target names] {
    run_binary_test $bin_file $load_address $entry_point $t
  }
}

proc run_elf_tests { elf_file entry_point } {
  safeReset
  testAllTargetsRunning

  run_elf_test $elf_file $entry_point ""
  foreach t [target names] {
    run_elf_test $elf_file $entry_point $t
  }
}

proc check_semihosting_was_enabled { Info } {
  if {[string trim $Info] ne "semihosting is enabled"} {
    error "could not enable semihosting: $Info"
  }
}

proc check_semihosting_was_disabled { Info } {
  if {[string trim $Info] ne "semihosting is disabled"} {
    error "could not disable semihosting: $Info"
  }
}

# NOTE: this test does not really check if side effects of semihosting
# enabling/disabling are actually observed.
proc run_psedo_semihosting_test { } {
  check_semihosting_was_enabled [sc_fpga_setup_semihosting]
  check_semihosting_was_disabled [sc_fpga_setup_semihosting disable]

  check_semihosting_was_enabled  [sc_fpga_setup_semihosting enable]
  check_semihosting_was_disabled  [sc_fpga_setup_semihosting disable]

  check_semihosting_was_enabled  [sc_fpga_setup_semihosting enable]
  check_semihosting_was_disabled  [sc_fpga_setup_semihosting stop]
}

proc run_test_with_elf_and_bin { bin elf load_address entry_point } {
  set load_address [expr $load_address]
  set entry_point [expr $entry_point]
  if {[catch { run_basic_tests } r] } {
    echo "basic tests failed: $r"
    shutdown error
  }

  if {[catch { run_binary_tests $bin $load_address $entry_point } r]} {
    echo "bin-run tests failed: $r"
    shutdown error
  }

  if {[catch { run_elf_tests $elf $entry_point } r] } {
    echo "elf-run tests failed: $r"
    shutdown error
  }

  if {[catch { run_psedo_semihosting_test } r]} {
    echo "unexpected error during semihosting enable/disable: $r"
    shutdown error
  }

  echo "Great Success"
  shutdown
}
