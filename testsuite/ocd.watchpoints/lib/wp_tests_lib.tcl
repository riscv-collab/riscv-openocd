namespace eval WP_TESTS {

proc offsets_b {} {
  return [list \
    offset_0 offset_1 offset_2 offset_3 \
    offset_4 offset_5 offset_6 offset_7 \
  ]
}

proc offsets_h {} {
  return [list \
    offset_0 offset_2 offset_4 offset_6 \
  ]
}

proc offsets_w {} {
  return [list \
    offset_0 offset_4 \
  ]
}

proc offsets_d {} {
  return [list \
    offset_0 \
  ]
}

proc wp_test_check_state {states expected_state i} {
  set state [lindex $states $i]
  if {![string match $state $expected_state]} {
    error "unexpected state of hart $i: $state, expecting $expected_state"
  }
}

proc wp_setup { target_idx address length { access "" } { value "" } { mask "" } } {
  ocdtlb_run_expect "halt"
  ocdtlb_run_expect "targets $target_idx"
  set wp_command [string trim "wp $address $length $access $value $mask"]
  verbose "setting up wp: $wp_command" 1
  ocdtlb_run_expect "$wp_command"
}

proc wp_clear { target_idx address } {
  ocdtlb_run_expect "halt"
  ocdtlb_run_expect "targets $target_idx"
  ocdtlb_run_expect "rwp $address"
}

proc wp_test_resume_and_run {target_idx start_pc end_pc expected_state } {
  ocdtlb_run_expect "halt"
  ocdtlb_run_expect "targets $target_idx"
  ocdtlb_run_expect "resume $start_pc"
  if { $expected_state eq "halted" } {
    ocdtlb_wait_resume wait_halt
  } else {
    ocdtlb_wait_resume
  }
  set response [ocdtlb_run_expect "targets"]
  wp_test_check_state [dict get $response states] $expected_state $target_idx
  ocdtlb_run_expect "halt"
  set pc [ocdtlb_rsp_get [ocdtlb_run_expect "reg pc"]]
  ocdtlb_expect_equal_hex $pc $end_pc
}

proc wp_test_run { target_idx wp_address ref_size access_type value mask start_pc end_pc end_state} {

  verbose [join [list "Testing WP($end_state): at " \
                      "$wp_address, " \
                      "sz: $ref_size, " \
                      "a: $access_type, " \
                      "value: $value, " \
                      "mask: $mask, " \
                      "code: $start_pc - $end_pc," \
                      "end_state: $end_state"] " "] 1

  wp_setup $target_idx $wp_address $ref_size $access_type $value $mask
  wp_test_resume_and_run $target_idx $start_pc $end_pc $end_state
  wp_clear $target_idx $wp_address
}

}
