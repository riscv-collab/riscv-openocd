proc translation_v2p_mode_test {v2p_mode} {
    if {[target_info cpu,xlen] != 64} {
        return [ocd_test_unsupported]
    }

    set lib_translate_support [ocdtlb_test_resource_path lib_translate_support.tcl]
    source $lib_translate_support

    set default_bin_prefix [tool_state_get default_bin_prefix]
    ocd_compile_executable "lib/memory.S" "$default_bin_prefix.exe"

    set ENTRY_ADDR [tool_state_get_program_symbol _entry]

    set expected_data 0xdeadbeef

    set available_modes [list sv39 sv48 sv57]
    set modes [list]

    foreach mode $available_modes {
        if {[target_info "cpu,$mode"]} {
            lappend modes $mode
        }
    }

    if {[llength $modes] == 0} {
        return [ocd_test_unsupported]
    }

    set table_addr $ENTRY_ADDR

    set va_offset 0xa0000
    set virtual_address 0x[format %x [expr {$table_addr + $va_offset}]]

    set pa_offset 0x10000
    set physical_address 0x[format %x [expr {$table_addr + $pa_offset}]]

    set va_list [list $virtual_address]
    set pa_list [list $physical_address]

    ocd_connect

    ocdtlb_run_expect "riscv virt2phys_mode $v2p_mode"

    foreach mode $modes {
        verbose "testing $mode translation mode" 1

        ocdtlb_run_expect "reset halt"

        init_with_config $mode $table_addr $va_list $pa_list
        turn_on_translation $mode $table_addr

        ocdtlb_run_expect "mww phys $virtual_address 0"
        ocdtlb_run_expect "mww phys $physical_address 0"

        ocdtlb_run_expect "mww phys $physical_address $expected_data"
        set data [ocdtlb_rsp_get [ocdtlb_run_expect "mdw $virtual_address"] data]
        ocdtlb_expect_equal $data $expected_data

        ocdtlb_run_expect "mww phys $virtual_address 0"
        ocdtlb_run_expect "mww phys $physical_address 0"

        ocdtlb_run_expect "mww $virtual_address $expected_data"
        set data [ocdtlb_rsp_get [ocdtlb_run_expect "mdw phys $physical_address"] data]
        ocdtlb_expect_equal $data $expected_data
    }

    return [ocd_test_pass]
}
