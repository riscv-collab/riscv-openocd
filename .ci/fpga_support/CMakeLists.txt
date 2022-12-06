cmake_minimum_required(VERSION 3.16)

project(FpgaSupport)

set(SYNTACORE_NAS_SERVER $ENV{SYNTACORE_NAS_SERVER})
set(ARTY_BITSTREAM_FTP_STORAGE ${SYNTACORE_NAS_SERVER}/pub/exchange/ot-sc/arty-a7-100t)

set(ARTY100_BOARDS
  arty100_scr1_32:scr1_\#18494.rar
  arty100_scr3_32:rv32_single_scr3_tcm_ipic_hiperf.rar
  arty100_scr4_32_imcaf:rv32_single_scr4_tcm_l1_ipic_hiperf_256MB_wo_D.tar.gz
  arty100_scr4_32_imcafd:rv32_single_scr4_tcm_l1_ipic_hiperf_256MB.tar.gz
)

set(TWIN_BOARDS
  twin_scr7_64_single_core:vcu118_scr7_hpfpu_enabled
  twin_scr7_64:rv64_cluster_scr7_eval
  twin_scr7_64_mcore4:rv64_cluster_scr7_eval
  twin_scr7_64_mcore2:rv64_dual_scr7_pci_storage_hpfpu
  twin_scr7_64_smp4:rv64_cluster_scr7_eval
  twin_scr7_64_wa:rv64_cluster_scr7_eval
  twin_scr5_32:rv32_cluster_single_scr5_l1_l2_plic_hiperf
  twin_scr5_64:rv64_cluster_single_scr5_tcm_l1_l2_plic_hiperf
  twin_scr6:rv64_cluster_single_scr6_eval
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

foreach(board ${TWIN_BOARDS})
  string(REGEX MATCHALL "([^:]+)" M ${board})

  list(GET M 0 BOARD_NAME)
  list(GET M 1 BITSTREAM)
  message("Configuring board-init for board<${BOARD_NAME}>, "
          "with bitstream: <${BITSTREAM}>")

  add_custom_target(board_${BOARD_NAME}
    WORKING_DIRECTORY /home/stand/fpga-runner-stand
    COMMAND
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/runner_inst.exp
        twin ${BITSTREAM}
  )
endforeach()
