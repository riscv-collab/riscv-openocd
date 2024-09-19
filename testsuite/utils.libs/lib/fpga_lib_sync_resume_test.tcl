proc run_test {} {
  reset halt
  if {[catch {sc_experimental_sync_resume {*}[target names]} err]} {
    if {$err eq "Only the case of one TAP per target is supported."} {
      shutdown
    }
    error $err
  }
  foreach target [target names] {
    targets $target
    poll
    if {[$target debug_reason] ne "target-not-halted"} {
      error "$target is not resumed!"
    }
  }
  shutdown
}
