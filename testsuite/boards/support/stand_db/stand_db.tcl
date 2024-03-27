proc standInfoGet { parameter } {
  global ADAPTER_INFO
  if { ![info exists ADAPTER_INFO] } {
    global OPENOCD_DEBUG_ADAPTER_CONFIG
    global OPENOCD_DEBUG_ADAPTER_SERIAL
    global OPENOCD_DEBUG_ADAPTER_SPEED
    global OPENOCD_ROOT

    if { ${OPENOCD_DEBUG_ADAPTER_CONFIG} eq "" } {
      puts "Debug Adapter Configuration is not defined for the target board"
      puts "Please, check OPENOCD_DEBUG_ADAPTER_CONFIG variable"
      error "OPENOCD_DEBUG_ADAPTER_CONFIG is empty"
    }

    if { ${OPENOCD_DEBUG_ADAPTER_SERIAL} eq "" } {
      puts "Debug Adapter Serial is not defined for the target board"
      puts "Please, check OPENOCD_DEBUG_ADAPTER_SERIAL variable"
      error "OPENOCD_DEBUG_ADAPTER_SERIAL is empty"
    }

    if { ${OPENOCD_DEBUG_ADAPTER_SPEED} eq "" } {
      puts "Debug Adapter Speed is not defined for the target board"
      puts "Please, check OPENOCD_DEBUG_ADAPTER_SPEED variable"
      error "OPENOCD_DEBUG_ADAPTER_SPEED is empty"
    }

    set ADAPTER_INFO(default_speed) ${OPENOCD_DEBUG_ADAPTER_SPEED}
    set ADAPTER_INFO(adapter_id) ${OPENOCD_DEBUG_ADAPTER_SERIAL}
    if { [file pathtype ${OPENOCD_DEBUG_ADAPTER_CONFIG}] eq "absolute" } {
      set ADAPTER_INFO(adapter_config) "${OPENOCD_DEBUG_ADAPTER_CONFIG}"
      set ADAPTER_INFO(adapter_config_relative) ""
      set ROOT_TEST ${OPENOCD_ROOT}
      if { [string match */ $ROOT_TEST] == 0 } {
        set ROOT_TEST "${ROOT_TEST}/"
      }
      if { [string first ${ROOT_TEST} ${OPENOCD_DEBUG_ADAPTER_CONFIG}] == 0} {
        set cut_from [string length ${ROOT_TEST}]
        set ADAPTER_INFO(adapter_config_relative) \
          [string range ${OPENOCD_DEBUG_ADAPTER_CONFIG} $cut_from end]
      }
    } else {
      set ADAPTER_INFO(adapter_config) \
        [file join ${OPENOCD_ROOT} ${OPENOCD_DEBUG_ADAPTER_CONFIG}]
      set ADAPTER_INFO(adapter_config_relative) ${OPENOCD_DEBUG_ADAPTER_CONFIG}
    }
  }
  return $ADAPTER_INFO($parameter)
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

  return [list \
    -f "[standInfoGet adapter_config]" \
    -c "transport select jtag" \
    -c "adapter speed [standInfoGet default_speed]" \
    -c "adapter serial [standInfoGet adapter_id]" \
  ]
}
