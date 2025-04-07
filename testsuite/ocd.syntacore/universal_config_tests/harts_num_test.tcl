init

echo [targets]
set names [target names]

set n_detected [llength [target names]]
if { $n_detected != $NCORES } {
  echo "unexpected number of detected targets $n_detected"
  shutdown error
}

set CHIPNAME riscv

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
