set CHIPNAME best_chip_evar
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

shutdown
