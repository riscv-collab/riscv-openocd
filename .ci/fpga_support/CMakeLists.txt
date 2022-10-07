cmake_minimum_required(VERSION 3.16)

project(FpgaSupport)

set(ARTY_BITSTREAM_FTP_STORAGE 192.168.1.100/pub/exchange/ot-sc/arty-a7-100t)

set(ARTY100_BOARDS
  arty100_scr1_32:scr1_\#18494.rar
  arty100_scr3_32:rv32_single_scr3_tcm_ipic_hiperf.rar
  arty100_scr4_32_imcaf:rv32_single_scr4_tcm_l1_ipic_hiperf_256MB_wo_D.tar.gz
  arty100_scr4_32_imcafd:rv32_single_scr4_tcm_l1_ipic_hiperf_256MB.tar.gz
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/BoardInit)

foreach(board ${ARTY100_BOARDS})
  string(REGEX MATCHALL "([^:]+)" M ${board})

  list(GET M 0 BOARD_NAME)
  list(GET M 1 BITSTREAM)
  message("Configuring board-init for board<${BOARD_NAME}>, "
          "with bitstream: <${BITSTREAM}>")

  add_custom_target(board_${BOARD_NAME}
    WORKING_DIRECTORY BoardInit
    COMMAND
      env BITSTREAM_FTP_STORAGE=${ARTY_BITSTREAM_FTP_STORAGE}
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/fetch_arty_bitstream.sh
        ${BITSTREAM}
    COMMAND
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/arty_inst.exp
        ${BITSTREAM}.bitstream.bit
  )
endforeach()
