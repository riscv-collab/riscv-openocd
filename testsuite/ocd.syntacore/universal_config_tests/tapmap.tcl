proc test_successfull_tapmap {chipname topostring tap_expect targets_expected } {
  echo "checking success on: $topostring"
  set tapmap [sc_target_build_tap_map $chipname $topostring]
  set taps [lindex $tapmap 0]
  set targets [lindex $tapmap 1]
  if {"${taps}" ne $tap_expect} {
    error "unexpected tap chain from tapmap: ${taps} vs expected ${tap_expect}"
  }
  if {"${targets}" ne $targets_expected} {
    error "unexpected targets from tapmap: ${targets} vs expected ${targets_expected}"
  }
}

proc test_tapmap_error {chipname topostring expected_err} {
  echo "checking error on: $topostring"
  if {[catch {sc_target_build_tap_map $chipname $topostring} err] == 0} {
    error "unexpected success: $err"
  }
  if {$err ne $expected_err} {
    error "error mismatch: $err vs expected $expected_err"
  }
}

test_successfull_tapmap blah 1riscvl3 \
  "{prefix tap suffix 0 irlen 3}" \
  "{name blah.cpu0 coreid -1 tapname tap.0}"
test_successfull_tapmap bleh 2riscvl4 \
  "{prefix tap suffix 0 irlen 4}" \
  "{name bleh.cpu0 coreid 0 tapname tap.0} {name bleh.cpu1 coreid 1 tapname tap.0}"
test_successfull_tapmap blah 2riscvl5 \
  "{prefix tap suffix 0 irlen 5}" \
  "{name blah.cpu0 coreid 0 tapname tap.0} {name blah.cpu1 coreid 1 tapname tap.0}"
test_successfull_tapmap keke 1riscvl5:1riscvl5 \
  "{prefix tap suffix 0 irlen 5} {prefix tap suffix 1 irlen 5}" \
  "{name keke.cpu0 coreid -1 tapname tap.0} {name keke.cpu1 coreid -1 tapname tap.1}"
test_successfull_tapmap kekeke 1l1:2riscvl4 \
  "{prefix tap suffix 0 irlen 1} {prefix tap suffix 1 irlen 4}" \
  "{name kekeke.cpu0 coreid 0 tapname tap.1} {name kekeke.cpu1 coreid 1 tapname tap.1}"
test_successfull_tapmap kekekeke riscv1l4:2riscvl4 \
  "{prefix tap suffix 0 irlen 4} {prefix tap suffix 1 irlen 4}" \
  "{name kekekeke.cpu0 coreid -1 tapname tap.0} {name kekekeke.cpu1 coreid 0 tapname tap.1} {name kekekeke.cpu2 coreid 1 tapname tap.1}"
test_successfull_tapmap kekekekekeke 1riscvl3:1l2 \
  "{prefix tap suffix 0 irlen 3} {prefix tap suffix 1 irlen 2}" \
  "{name kekekekekeke.cpu0 coreid -1 tapname tap.0}"

test_successfull_tapmap argh riscv1l3 \
  "{prefix tap suffix 0 irlen 3}" \
  "{name argh.cpu0 coreid -1 tapname tap.0}"
test_successfull_tapmap urgh riscv2l4 \
  "{prefix tap suffix 0 irlen 4} {prefix tap suffix 1 irlen 4}" \
  "{name urgh.cpu0 coreid -1 tapname tap.0} {name urgh.cpu1 coreid -1 tapname tap.1}"
test_successfull_tapmap ouch 3l1:riscv1l3 \
  "{prefix tap suffix 0 irlen 1} {prefix tap suffix 1 irlen 1} {prefix tap suffix 2 irlen 1} {prefix tap suffix 3 irlen 3}" \
  "{name ouch.cpu0 coreid -1 tapname tap.3}"
test_successfull_tapmap ooph riscv2l4:2l1 \
  "{prefix tap suffix 0 irlen 4} {prefix tap suffix 1 irlen 4} {prefix tap suffix 2 irlen 1} {prefix tap suffix 3 irlen 1}" \
  "{name ooph.cpu0 coreid -1 tapname tap.0} {name ooph.cpu1 coreid -1 tapname tap.1}"
test_successfull_tapmap pooh 1l1:riscv2l4:2l1 \
  "{prefix tap suffix 0 irlen 1} {prefix tap suffix 1 irlen 4} {prefix tap suffix 2 irlen 4} {prefix tap suffix 3 irlen 1} {prefix tap suffix 4 irlen 1}" \
  "{name pooh.cpu0 coreid -1 tapname tap.1} {name pooh.cpu1 coreid -1 tapname tap.2}"
test_successfull_tapmap pluh 1l1:riscv1l4:2l1 \
  "{prefix tap suffix 0 irlen 1} {prefix tap suffix 1 irlen 4} {prefix tap suffix 2 irlen 1} {prefix tap suffix 3 irlen 1}" \
  "{name pluh.cpu0 coreid -1 tapname tap.1}"
test_successfull_tapmap notargets "1l3" \
  "{prefix tap suffix 0 irlen 3}" \
  ""

test_tapmap_error Timmy! [join [lrepeat 11 1024l1] :] \
  "number of tap chain atoms (11264) is above limit 10240 (1024l1:1024l1:1024l1:1024l1:1024l1:1024l1:1024l1:1024l1:1024l1:1024l1:1024l1)"
test_tapmap_error Tommy! 1025l1 \
  "number of taps (1025) in *1025l1* atom is too large"
test_tapmap_error Timmy! riscv1025l5 \
  "number of taps (1025) in *riscv1025l5* atom is too large"
test_tapmap_error Tommy! 1025riscvl5 \
  "number of targets (1025) in *1025riscvl5* atom is too large"
test_tapmap_error Timmy! riscvl5 \
  "incorrect jtag topology atom format *riscvl5*"
test_tapmap_error Tommy! riscv1l0 \
  "IRLEN is zero in topology atom *riscv1l0*"
test_tapmap_error Timmy! 1l0 \
  "IRLEN is zero in topology atom *1l0*"
test_tapmap_error Tommy! riscv1l33 \
  "IRLEN 33 is too large in topology atom *riscv1l33*"
test_tapmap_error Timmy! 1l33 \
  "IRLEN 33 is too large in topology atom *1l33*"
test_tapmap_error Tommy! "" \
  "no taps are derived from the input topology ()"

shutdown

