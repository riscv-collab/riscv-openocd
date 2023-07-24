proc pmu_selectors {} {
  return [dict create \
    COUNTER1  1 \
    COUNTER2  2 \
    COUNTER3  3 \
    COUNTER4  4 \
    COUNTER5  5 \
    COUNTER6  6 \
    COUNTER7  7 \
    COUNTER8  8 \
    COUNTER9  9 \
 ]
}

proc run_pmu_test {ElfFile Entry} {
  echo "====TEST INITIALIZATION==="
  reset halt
  if {[catch {
    load_image $ElfFile
    resume $Entry
  } err]} {
    echo "unexpected error during initialization: $err"
    shutdown error
  }

  echo "====TESTING HALTED REQUIREMENT FOR PMU SETUP==="
  if {![catch {sc_experimental_pmu_setup [pmu_selectors] 4 CY IR } err]} {
    echo "sc_experimental_pmu_setup is expected to target being running"
    shutdown error
  }
  halt
  set CTX ""
  if {[catch {sc_experimental_pmu_setup [pmu_selectors] 4 CY IR } CTX]} {
    echo "sc_experimental_pmu_setup should succeed on a halted target"
    shutdown error
  }
  resume
  if {![catch {sc_experimental_pmu_get $CTX CY} err]} {
    echo "sc_experimental_pmu_get should fail on a runnig target"
    shutdown error
  }
  echo "$err (expected)"

  halt
  echo "====TESTING ERROR DETECTION==="
  if {![catch {sc_experimental_pmu_setup [pmu_selectors] 4 CY IR COUNTER1 COUNTER2 COUNTER3 COUNTER4 COUNTER5} err]} {
    echo "sc_experimental_pmu_setup is expected to fail due to large number of counters"
    shutdown error
  }
  echo "$err (expected)"

  if {![catch {sc_experimental_pmu_setup [pmu_selectors] 333 CY IR COUNTER1 COUNTER2 COUNTER3 COUNTER4} err]} {
    echo "sc_experimental_pmu_setup is expected to fail due to incorrect PMU counters limit"
    shutdown error
  }
  echo "$err (expected)"

  if {![catch {sc_experimental_pmu_setup [pmu_selectors] CY CY IR COUNTER1 COUNTER2 COUNTER3 COUNTER4} err]} {
    echo "sc_experimental_pmu_setup is expected to fail due to incorrect PMU counters limit"
    shutdown error
  }
  echo "$err (expected)"

  set CTX [sc_experimental_pmu_setup [pmu_selectors] 4 CY IR]
  sc_experimental_pmu_get $CTX CY
  sc_experimental_pmu_get $CTX IR
  sc_experimental_pmu_get $CTX TIME
  if {![catch {sc_experimental_pmu_get $CTX COUNTER1} err]} {
    echo "sc_experimental_pmu_setup is expected to fail due to unexpected PMU counter"
    shutdown error
  }
  echo "$err (expected)"

  echo "====TESTING EMPTY PMU COUNTERS==="
  set CTX [sc_experimental_pmu_setup [pmu_selectors] 4]
  # NOTE: we always allow reading CY/IR/TIME
  sc_experimental_pmu_get $CTX CY
  sc_experimental_pmu_get $CTX IR
  sc_experimental_pmu_get $CTX TIME
  echo "====TESTING ONE PMU COUNTER==="
  set CTX [sc_experimental_pmu_setup [pmu_selectors] 4 COUNTER1]
  sc_experimental_pmu_get $CTX CY
  sc_experimental_pmu_get $CTX IR
  sc_experimental_pmu_get $CTX TIME
  sc_experimental_pmu_get $CTX COUNTER1
  echo "====TESTING 4 PMU COUNTERS==="
  set CTX [sc_experimental_pmu_setup [pmu_selectors] 4 COUNTER1 COUNTER2 COUNTER3 COUNTER4]
  sc_experimental_pmu_get $CTX CY
  sc_experimental_pmu_get $CTX IR
  sc_experimental_pmu_get $CTX TIME
  sc_experimental_pmu_get $CTX COUNTER1
  sc_experimental_pmu_get $CTX COUNTER2
  sc_experimental_pmu_get $CTX COUNTER3
  sc_experimental_pmu_get $CTX COUNTER4
  echo "====TESTING 4 PMU COUNTERS with CY and IR==="
  set CTX [sc_experimental_pmu_setup [pmu_selectors] 4 CY IR COUNTER1 COUNTER2 COUNTER3 COUNTER4]
  sc_experimental_pmu_get $CTX CY
  sc_experimental_pmu_get $CTX IR
  sc_experimental_pmu_get $CTX TIME
  sc_experimental_pmu_get $CTX COUNTER1
  sc_experimental_pmu_get $CTX COUNTER2
  sc_experimental_pmu_get $CTX COUNTER3
  sc_experimental_pmu_get $CTX COUNTER4
  shutdown
}

