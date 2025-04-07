set NCORES $::env(NCORES)
set IS_SIM $::env(IS_SIM)

if { $IS_SIM == 1 } {
  sc_target_config jtag_topology ${NCORES}riscvl5
} else {
  sc_target_config jtag_topology riscv${NCORES}l5
}
set CHIPNAME best_chip_evar_topo_test
sc_target_config chipname $CHIPNAME

init

echo [targets]
set names [target names]

set ind 0
echo $names
foreach name $names {
  if { [string first $CHIPNAME.cpu$ind $name] != 0 } {
    echo "ERROR: unexpected chipname <$name>!"
    shutdown error
  }
  incr ind
}

set n_detected [llength [target names]]
if { $n_detected != $NCORES } {
  echo "unexpected number of detected targets $n_detected"
  shutdown error
}

shutdown
