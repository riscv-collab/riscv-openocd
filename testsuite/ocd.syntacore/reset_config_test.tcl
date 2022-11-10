sc_target_config reset_config trst_and_srst

init

set RST [reset_config]
echo $RST

if { [string first trst_and_srst $RST] == -1 } {
  echo "unexpected reset_config: $RST"
  shutdown error
}

shutdown
