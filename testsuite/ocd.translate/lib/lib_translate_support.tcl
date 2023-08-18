proc generate_page_table { CONFIG_NAME } {
    set default_bin_prefix [tool_state_get default_bin_prefix]
    set generator_path [ocdtlb_test_resource_path mmu_gen.c]
    append section_source_path $default_bin_prefix "_section.c"

    append generator $default_bin_prefix "_mmu_gen.exe"
    append section_file $default_bin_prefix "_section.exe"
    set section_name ".data"

    append page_table_file $CONFIG_NAME "_page_table.bin"
    append pte_header $CONFIG_NAME "_pte.h"

    exec gcc -w -O0 -fno-unroll-loops -std=c2x $generator_path -o $generator
    verbose "[exec ./$generator $CONFIG_NAME $pte_header]" 1

    append section_source_content "#include <stdio.h>\n"
    append section_source_content "#include <stdint.h>\n"
    append section_source_content "#include \"$pte_header\""
    exec echo $section_source_content >$section_source_path

    exec gcc -c $section_source_path -o $section_file
    exec objcopy --dump-section $section_name=$page_table_file $section_file

    return $page_table_file
}

proc generate_config { mode va pa table_addr } {
    set default_bin_prefix [tool_state_get default_bin_prefix]
    append config $default_bin_prefix "_" $mode ".config"

    append config_content $mode "\n"
    append config_content $table_addr "\n"
    append config_content "$va,$pa,0,0xff\n"

    exec echo $config_content >$config

    return $config
}

proc init_with_config { mode table_addr } {
    set va_offset 0xa000
    set virtual_address 0x[format %x [expr {$table_addr + $va_offset}]]

    set pa_offset 0xb000
    set physical_address 0x[format %x [expr {$table_addr + $pa_offset}]]

    set config [generate_config $mode $virtual_address $physical_address $table_addr]

    set page_table_file [generate_page_table $config]

    ocdtlb_run_expect "reset halt"

    ocdtlb_run_expect \
      "load_image $page_table_file $table_addr bin" expect_success [target_info env,long_op_timeout]

    switch $mode {
        sv39 {set mode_val 8}
        sv48 {set mode_val 9}
        sv57 {set mode_val 10}
        default {
            error "Incorrect translation mode"
        }
    }

    set SATP_MODE [expr {$mode_val << 60}]
    set RISCV_PGSHIFT 12
    set SATP_PPN [expr {$table_addr >> $RISCV_PGSHIFT}]

    set SATP_VALUE 0x[format %x [expr {$SATP_MODE | $SATP_PPN}]]

    ocdtlb_run_expect "reg satp $SATP_VALUE"

    ocdtlb_run_expect "reg priv 0x1"

    return [dict create va $virtual_address pa $physical_address]
}
