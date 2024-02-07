proc registerStandInfo { stand_name adapter_config adapter_id default_speed } {
  global STANDS_INFO
  set STANDS_INFO($stand_name,adapter_config) $adapter_config
  set STANDS_INFO($stand_name,adapter_id) $adapter_id
  set STANDS_INFO($stand_name,default_speed) $default_speed
}

proc standInfoGet { parameter } {
  global STANDS_INFO
  set stand_name [target_info env,stand]
  return $STANDS_INFO($stand_name,$parameter)
}

proc standInfoGetInterfaceFlags { } {
  if {[target_info sim] ne ""} {
    set rbb_port [spike_get_active_rbb_port]
    return [list \
      -c "telnet_port disabled" \
      -c "adapter driver remote_bitbang" \
      -c "remote_bitbang host 127.0.0.1" \
      -c "remote_bitbang port $rbb_port" \
      -c "tcl_port 0" \
      -c "gdb_port 0" \
    ]
  }

  global OPENOCD_ROOT
  return [list \
    -f "${OPENOCD_ROOT}/[standInfoGet adapter_config]" \
    -c "transport select jtag" \
    -c "adapter speed [standInfoGet default_speed]" \
    -c "adapter serial [standInfoGet adapter_id]" \
  ]
}
