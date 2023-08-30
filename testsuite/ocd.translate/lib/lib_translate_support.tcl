proc compile_page_table_generator {} {
    set default_bin_prefix [tool_state_get default_bin_prefix]
    set generator_path [ocdtlb_test_resource_path mmu_gen.c]
    append generator $default_bin_prefix "_mmu_gen.exe"
    exec gcc -w -O0 -fno-unroll-loops -std=c2x $generator_path -o $generator
}

proc generate_page_table { CONFIG_NAME } {
    set default_bin_prefix [tool_state_get default_bin_prefix]
    append section_source_path $default_bin_prefix "_section.c"

    append generator $default_bin_prefix "_mmu_gen.exe"
    append section_file $default_bin_prefix "_section.exe"
    set section_name ".data"

    append page_table_file $CONFIG_NAME "_page_table.bin"
    append pte_header $CONFIG_NAME "_pte.h"

    verbose "[exec ./$generator $CONFIG_NAME $pte_header]" 1

    append section_source_content "#include <stdio.h>\n"
    append section_source_content "#include <stdint.h>\n"
    append section_source_content "#include \"$pte_header\""
    exec echo $section_source_content >$section_source_path

    exec gcc -c $section_source_path -o $section_file
    exec objcopy --dump-section $section_name=$page_table_file $section_file

    return $page_table_file
}

proc generate_config { mode va_list pa_list table_addr } {
    if {[llength $va_list] != [llength $pa_list]} {
        error "va_list and pa_list should be equal"
    }

    set default_bin_prefix [tool_state_get default_bin_prefix]
    append config $default_bin_prefix "_" $mode ".config"

    append config_content $mode "\n"
    append config_content $table_addr "\n"

    foreach va $va_list pa $pa_list {
        append config_content "$va,$pa,0,0xff\n"
    }

    exec echo $config_content >$config

    return $config
}

proc init_with_config { mode table_addr va_list pa_list} {
    set config [generate_config $mode $va_list $pa_list $table_addr]

    set page_table_file [generate_page_table $config]

    ocdtlb_run_expect "reset halt"

    ocdtlb_run_expect \
      "load_image $page_table_file $table_addr bin" expect_success [target_info env,long_op_timeout]
}

proc turn_on_translation { mode table_addr {use_vsatp 0}} {
    switch $mode {
        sv39 {
            set mode_val 8
            set mode_x4 0
        }
        sv48 {
            set mode_val 9
            set mode_x4 0
        }
        sv57 {
            set mode_val 10
            set mode_x4 0
        }
        sv39x4 {
            set mode_val 8
            set mode_x4 1
        }
        sv48x4 {
            set mode_val 9
            set mode_x4 1
        }
        sv57x4 {
            set mode_val 10
            set mode_x4 1
        }
        default {
            error "Incorrect translation mode"
        }
    }

    if {$mode_x4 == 0} {
        if {$use_vsatp == 0} {
            set PT_REG satp
            set PRIV_VALUE 0x1
        } else {
            set PT_REG vsatp
            set PRIV_VALUE 0x5
        }
    } else {
        if {$use_vsatp == 1} {
            error "Can't use vsatp for x4 mode"
        }

        set PT_REG hgatp
        set PRIV_VALUE 0x5
    }

    set PT_REG_MODE [expr {$mode_val << 60}]
    set RISCV_PGSHIFT 12
    set PT_REG_PPN [expr {$table_addr >> $RISCV_PGSHIFT}]

    set PT_REG_VALUE 0x[format %x [expr {$PT_REG_MODE | $PT_REG_PPN}]]

    ocdtlb_run_expect "reg $PT_REG $PT_REG_VALUE"

    ocdtlb_run_expect "reg priv $PRIV_VALUE"
}
