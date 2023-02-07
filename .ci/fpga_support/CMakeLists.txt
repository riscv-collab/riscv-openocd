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
  twin_scr5_32:rv32_cluster_single_scr5_l1_l2_plic_hiperf
  twin_scr5_64:rv64_cluster_single_scr5_tcm_l1_l2_plic_hiperf
  twin_scr6:rv64_cluster_single_scr6_eval
  twin_scr7evalcluster_pseudoscore_nortos:rv64_cluster_scr7_eval
  twin_scr7evalcluster_pseudoscore_nortos_workarea:rv64_cluster_scr7_eval
  twin_scr7evalcluster_mcore4_nortos_workarea:rv64_cluster_scr7_eval
  twin_scr7evalcluster_mcore4_nortos:rv64_cluster_scr7_eval
  twin_scr7evalcluster_smp4_workarea:rv64_cluster_scr7_eval
  twin_scr7evalcluster_smp4:rv64_cluster_scr7_eval
  twin_scr7bug21107_score_nortos:/home/stand/users/aap-sc/bitstreams/bug_21107/1/22121300/top.bit
  twin_scr7RVV_mcore2_nortos:/home/stand/users/aap-sc/bitstreams/rvv_openocd/ver1/top.bit
  twin_SCR7dev_mcore2_nortos:SCR7dev
  twin_SCR7dev_mcore2_rtoshw:SCR7dev
  twin_SCR9dev_score_nortos:SCR9dev
  twin_SCR9dev_score_rtoshw:SCR9dev
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
