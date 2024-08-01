init
halt
poll

set EBREAK_M_POS [expr {1 << 15}]
set EBREAK_S_POS [expr {1 << 13}]
set EBREAK_U_POS [expr {1 << 12}]
set EBREAK_BITS_MASK [expr { ${EBREAK_M_POS} | ${EBREAK_S_POS} | ${EBREAK_U_POS} }]

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

proc check_ebreak_enabled {} {
  global EBREAK_BITS_MASK
  set ebreak_bits_val [get_ebreak_bits]

  if { ${ebreak_bits_val} == ${EBREAK_BITS_MASK} } {
    shutdown
  }
  shutdown error
}
