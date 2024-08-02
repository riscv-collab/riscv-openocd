init
halt
poll

set EBREAK_M_POS_MASK [expr {1 << 15}]
set EBREAK_S_POS_MASK [expr {1 << 13}]
set EBREAK_U_POS_MASK [expr {1 << 12}]
set EBREAK_BITS_MASK \
  [expr { ${EBREAK_M_POS_MASK} | ${EBREAK_S_POS_MASK} | ${EBREAK_U_POS_MASK} }]

proc get_ebreak_bits {} {
  global EBREAK_BITS_MASK
  set DCSR_VAL [sc_fpga_read_reg dcsr]
  set EBREAK_BITS_VAL [expr { ${DCSR_VAL} & ${EBREAK_BITS_MASK} }]
  echo "====== EBREAK_BITS: ${EBREAK_BITS_VAL}"
  return ${EBREAK_BITS_VAL}
}

proc check_ebreak_disabled {} {
  set ebreak_bits_val [get_ebreak_bits]
  if { ${ebreak_bits_val} == 0 } {
    shutdown
  }
  shutdown error
}

proc check_ebreak_enabled {has_u_mode has_s_mode} {
  global EBREAK_M_POS_MASK
  global EBREAK_S_POS_MASK
  global EBREAK_U_POS_MASK
  global EBREAK_BITS_MASK
  set ebreak_bits_val [get_ebreak_bits]

  set expected_bits ${EBREAK_M_POS_MASK}
  if { $has_u_mode } {
    set expected_bits [ expr { $expected_bits | $EBREAK_U_POS_MASK } ]
  }
  if { $has_s_mode } {
    set expected_bits [ expr { $expected_bits | $EBREAK_S_POS_MASK } ]
  }

  if { ${ebreak_bits_val} == ${expected_bits} } {
    shutdown
  }
  shutdown error
}
