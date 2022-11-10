init

set n_detected [llength [target names]]
if { $n_detected != $NCORES } {
  echo "unexpected number of detected targets $n_detected"
  shutdown error
}

shutdown
