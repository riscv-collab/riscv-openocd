echo "----------------------------------"
echo "CONFIGURE-HOOK: pre-init printing"
echo "----------------------------------"
# Just some command which is legal only during CONFIG stage
gdb report_data_abort enable

proc sc_pre_tap_hook {} {
  echo "----------------------------------"
  echo "CONFIGURE-HOOK: sc_pre_tap_hook"
  echo "----------------------------------"
}

proc sc_target_configuration_hook { target } {
  echo "----------------------------------"
  echo "CONFIGURE-HOOK: sc_target_configuration_hook: target=$target"
  echo "----------------------------------"
}

proc sc_targets_ready_hook { target_list } {
  echo "----------------------------------"
  echo "CONFIGURE-HOOK: sc_targets_ready_hook: targets_num=[llength $target_list]"
  echo "----------------------------------"
}
