sc_target_config reset_config { trst_and_srst srst_nogate }

init

set RST [reset_config]

echo $RST

if { [string first trst_and_srst $RST] == -1 } {
  echo "unexpected reset_config: $RST"
  shutdown error
}

if { [string first srst_nogate $RST] == -1 } {
  echo "unexpected reset_config: $RST"
  shutdown error
}

shutdown
