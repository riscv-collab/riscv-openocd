set(SYNTACORE_NAS_SERVER nas.dev.syntacore.com)
set(ARTY_BITSTREAM_FTP_STORAGE ${SYNTACORE_NAS_SERVER}/pub/exchange/ot-sc/arty-a7-100t)

message(STATUS "Syntacore NAS server: ${SYNTACORE_NAS_SERVER}")

if (NOT OPENOCD_TESTSUITE_DIRECTORY)
  message(FATAL_ERROR "OPENOCD_TESTSUITE_DIRECTORY is not specified")
endif()

message(STATUS "OPENOCD_TESTSUITE_DIRECTORY: ${OPENOCD_TESTSUITE_DIRECTORY}")

add_library(FpgaBoardInfo INTERFACE)

function(registerFPGAConfiguration CONFIGURATION_NAME)
  set(options
      ARTY_STAND_TOOL
      BITSTREAM_FROM_NAS
      ZALMAN_NIGHTLY
      TWIN_NIGHTLY
      TWIN_SCR9
      TWIN_UNSTABLE
  )
  set(oneValueArgs
    BITSTREAM
    OPENOCD_BOARD
  )
  cmake_parse_arguments(PARSE_ARGV 1 BOARD_ARG
    "${options}" "${oneValueArgs}" ""
  )

  cmake_path(APPEND OPENOCD_TESTSUITE_DIRECTORY boards OUTPUT_VARIABLE BOARDS_DIR)
  cmake_path(APPEND BOARDS_DIR ${BOARD_ARG_OPENOCD_BOARD}.exp OUTPUT_VARIABLE BOARD_FILE)
  if (NOT EXISTS ${BOARD_FILE})
    message(FATAL_ERROR "could not find ${BOARD_FILE} board definition file")
  endif()

  set(INTERFACE_LIB_NAME fpga-${CONFIGURATION_NAME})

  add_library(${INTERFACE_LIB_NAME} INTERFACE)
  set_property(TARGET ${INTERFACE_LIB_NAME}
    PROPERTY OPENOCD_BOARD ${BOARD_ARG_OPENOCD_BOARD}
  )
  set_property(TARGET ${INTERFACE_LIB_NAME}
    PROPERTY BITSTREAM ${BOARD_ARG_BITSTREAM}
  )
  if (BOARD_ARG_ARTY_STAND_TOOL)
    set_property(TARGET ${INTERFACE_LIB_NAME} PROPERTY USE_ARTY_TOOLS ON)
  endif()

  if (BOARD_ARG_ZALMAN_NIGHTLY)
    set_property(TARGET ${INTERFACE_LIB_NAME} PROPERTY TESTING_CYCLE zalman APPEND)
    set_property(TARGET FpgaBoardInfo
      PROPERTY ZALMAN_NIGHTLY_CONFIGURATIONS ${INTERFACE_LIB_NAME} APPEND)
  endif()
  if (BOARD_ARG_TWIN_NIGHTLY)
    set_property(TARGET ${INTERFACE_LIB_NAME} PROPERTY TESTING_CYCLE twin_nightly APPEND)
    set_property(TARGET FpgaBoardInfo
      PROPERTY TWIN_NIGHTLY_CONFIGURATIONS ${INTERFACE_LIB_NAME} APPEND)
  endif()
  if (BOARD_ARG_TWIN_SCR9)
    set_property(TARGET ${INTERFACE_LIB_NAME} PROPERTY TESTING_CYCLE twin_scr9 APPEND)
    set_property(TARGET FpgaBoardInfo
      PROPERTY TWIN_SCR9_CONFIGURATIONS ${INTERFACE_LIB_NAME} APPEND)
  endif()
  if (BOARD_ARG_TWIN_UNSTABLE)
    set_property(TARGET ${INTERFACE_LIB_NAME} PROPERTY TESTING_CYCLE twin_unstable APPEND)
    set_property(TARGET FpgaBoardInfo
      PROPERTY TWIN_UNSTABLE_CONFIGURATIONS ${INTERFACE_LIB_NAME} APPEND)
  endif()

  get_property(testing_cycle TARGET ${INTERFACE_LIB_NAME} PROPERTY TESTING_CYCLE)
  if (NOT testing_cycle)
    message(FATAL_ERROR "could not derive testing cycle for ${INTERFACE_LIB_NAME}")
  endif()

  set_property(TARGET FpgaBoardInfo
    PROPERTY ALL_CONFIGURATIONS ${INTERFACE_LIB_NAME}
    APPEND
  )
endfunction()

function(addSCR7L2Config RELEASE_NAME BITSTREAM)
  set(options TWIN_NIGHTLY)
  cmake_parse_arguments(PARSE_ARGV 1 SCR7BOARD_ARG
    "${options}" "" ""
  )

  if (SCR7BOARD_ARG_TWIN_NIGHTLY)
    set(TESTING_CYCLE TWIN_NIGHTLY)
  else()
    set(TESTING_CYCLE TWIN_UNSTABLE)
  endif()

  set(CONFIG_BASENAME "twin_${RELEASE_NAME}")

  set(CONFIG_MCORE2_NORTOS "${CONFIG_BASENAME}_mcore2_nortos")
  set(CONFIG_MCORE2_RTOSHW "${CONFIG_BASENAME}_mcore2_rtoshw")
  set(CONFIG_SMP2 "${CONFIG_BASENAME}_smp2")

  registerFPGAConfiguration(${CONFIG_MCORE2_NORTOS}
    BITSTREAM ${BITSTREAM}
    OPENOCD_BOARD twin_scr7_x_mcore2_nortos
    ${TESTING_CYCLE}
  )
  registerFPGAConfiguration(${CONFIG_MCORE2_RTOSHW}
    BITSTREAM ${BITSTREAM}
    OPENOCD_BOARD twin_scr7_x_mcore2_rtoshw
    ${TESTING_CYCLE}
  )
  registerFPGAConfiguration(${CONFIG_SMP2}
    BITSTREAM ${BITSTREAM}
    OPENOCD_BOARD twin_scr7_x_smp2
    ${TESTING_CYCLE}
  )
endfunction()

function(addSCR9L2Config RELEASE_NAME BITSTREAM)
  registerFPGAConfiguration("twin_norvv_${RELEASE_NAME}_score_nortos"
    BITSTREAM ${BITSTREAM}
    OPENOCD_BOARD twin_scr9norvv_x_1core_nortos
    TWIN_SCR9
  )
  registerFPGAConfiguration("twin_norvv_${RELEASE_NAME}_score_rtoshw"
    BITSTREAM ${BITSTREAM}
    OPENOCD_BOARD twin_scr9norvv_x_1core_rtoshw
    TWIN_SCR9
  )
  registerFPGAConfiguration("twin_rvv_${RELEASE_NAME}_score_nortos"
    BITSTREAM ${BITSTREAM}
    OPENOCD_BOARD twin_scr9_x_1core_nortos
    TWIN_SCR9
  )
endfunction()

function(printList header the_list)
  message(STATUS "${header}")
  foreach(config ${the_list})
    message(STATUS "    ${config}")
  endforeach()
endfunction()

function(printFPGAConfigsForTestingCycle property)
  get_property(configurations TARGET FpgaBoardInfo PROPERTY ${property})
  printList("${property}:" "${configurations}")
endfunction()

function(printAvailableFPGAConfigurations)
  get_property(AVAILABLE_FPGA_CONFIGURATIONS
               TARGET FpgaBoardInfo
               PROPERTY ALL_CONFIGURATIONS)
  message(STATUS "AVAILABLE_FPGA_CONFIGURATIONS:")
  foreach(config ${AVAILABLE_FPGA_CONFIGURATIONS})
    get_property(bitstream TARGET ${config} PROPERTY BITSTREAM)
    get_property(openocd_board TARGET ${config} PROPERTY OPENOCD_BOARD)
    message(STATUS "    ${config}")
    message(STATUS "        bitstream: ${bitstream}")
    message(STATUS "        openocd_board: ${openocd_board}")
  endforeach()

  printFPGAConfigsForTestingCycle(ZALMAN_NIGHTLY_CONFIGURATIONS)
  printFPGAConfigsForTestingCycle(TWIN_NIGHTLY_CONFIGURATIONS)
  printFPGAConfigsForTestingCycle(TWIN_SCR9_CONFIGURATIONS)
  printFPGAConfigsForTestingCycle(TWIN_UNSTABLE_CONFIGURATIONS)
endfunction()

# ARTY-100 configs (for zalman stands)
registerFPGAConfiguration(arty100_scr1_32
  BITSTREAM_FROM_NAS
  BITSTREAM "scr1_#18494.rar"
  OPENOCD_BOARD arty100_scr1_32
  ARTY_STAND_TOOL
  ZALMAN_NIGHTLY
)
registerFPGAConfiguration(arty100_scr3_32
  BITSTREAM_FROM_NAS
  BITSTREAM rv32_single_scr3_tcm_ipic_hiperf.rar
  OPENOCD_BOARD arty100_scr3_32
  ARTY_STAND_TOOL
  ZALMAN_NIGHTLY
)
registerFPGAConfiguration(arty100_scr4_32_imcaf
  BITSTREAM_FROM_NAS
  BITSTREAM rv32_single_scr4_tcm_l1_ipic_hiperf_256MB_wo_D.tar.gz
  OPENOCD_BOARD arty100_scr4_32_imcaf
  ARTY_STAND_TOOL
  ZALMAN_NIGHTLY
)
registerFPGAConfiguration(arty100_scr4_32_imcafd
  BITSTREAM_FROM_NAS
  BITSTREAM rv32_single_scr4_tcm_l1_ipic_hiperf_256MB.tar.gz
  OPENOCD_BOARD arty100_scr4_32_imcafd
  ARTY_STAND_TOOL
  ZALMAN_NIGHTLY
)

# Twin stands
registerFPGAConfiguration(twin_scr5_32
  BITSTREAM scr5_rv32
  OPENOCD_BOARD twin_scr5_32
  TWIN_NIGHTLY
)
registerFPGAConfiguration(twin_scr5_64
  BITSTREAM scr5_rv64_tcm
  OPENOCD_BOARD twin_scr5_64
  TWIN_NIGHTLY
)
registerFPGAConfiguration(twin_scr6
  BITSTREAM rv64_cluster_single_scr6_eval
  OPENOCD_BOARD twin_scr6
  TWIN_NIGHTLY
)

# SCR7 testing

addSCR7L2Config(scr7_rls_ww08.2.1
  /home/stand/users/aap-sc/BITSTREAMS/SCR7WR_23WW08.2.1/SCR7WR_23WW08.2.1_Dual_65MHz.bit
  TWIN_NIGHTLY
)

addSCR7L2Config(scr7_rls_ww15.3.0
  /home/stand/users/aap-sc/BITSTREAMS/SCR7WR_23WW15.3.0/SCR7WR_23WW15.3.0.bit
  TWIN_NIGHTLY
)

addSCR7L2Config(scr7_rls_ww22.3.1
  /home/stand/users/aap-sc/BITSTREAMS/SCR7WR_23WW22.3.1/SCR7WR_23WW22.3.1_Dual_65MHz.bit
  TWIN_NIGHTLY
)

addSCR7L2Config(scr7_dev
  scr7_dev
)

# SCR9 testing

addSCR9L2Config(scr9_23WW14.5.0
  /home/stand/users/aap-sc/BITSTREAMS/scr9_rls_23ww14.5.0/scr9_rls_23ww14.5.0.bit
)

addSCR9L2Config(scr9_23WW18.5.0
  /home/stand/users/aap-sc/BITSTREAMS/scr9_rls_23ww18.5.0/scr9_rls_23ww18.5.0.bit
)

addSCR9L2Config(scr9_23WW25.3.0
  /home/stand/users/aap-sc/BITSTREAMS/scr9_rls_23ww25.3.0/scr9_rls_23ww25.3.0.bit
)

addSCR9L2Config(scr9_dev
  scr9_dev
)


# Legacy platforms (for historic interest)

addSCR7L2Config(scr7_rls_ww04.1.0
  /home/stand/users/aap-sc/BITSTREAMS/SCR7WR_23WW04.1.0/scr7_rls_ww04.1.0.bit
)
addSCR7L2Config(scr7_rls_ww05.4.0
  /home/stand/users/aap-sc/BITSTREAMS/SCR7WR_23WW08.2.1/SCR7WR_23WW08.2.1_Dual_65MHz.bit
)
addSCR7L2Config(scr7_rls_ww08.1.0
  /home/stand/users/aap-sc/BITSTREAMS/SCR7WR_23WW08.2.1/SCR7WR_23WW08.2.1_Dual_65MHz.bit
)

registerFPGAConfiguration(twin_scr7evalcluster_pseudoscore_nortos
  BITSTREAM scr7_l2_mpu
  OPENOCD_BOARD twin_scr7_x_1core_nortos
  TWIN_UNSTABLE
)
registerFPGAConfiguration(twin_scr7evalcluster_mcore4_nortos
  BITSTREAM scr7_l2_mpu
  OPENOCD_BOARD twin_scr7_x_mcore4_nortos
  TWIN_UNSTABLE
)
registerFPGAConfiguration(twin_scr7evalcluster_smp4_workarea
  BITSTREAM scr7_l2_mpu
  OPENOCD_BOARD twin_scr7_x_smp4_workarea
  TWIN_UNSTABLE
)
registerFPGAConfiguration(twin_scr7evalcluster_smp4
  BITSTREAM scr7_l2_mpu
  OPENOCD_BOARD twin_scr7_x_smp4
  TWIN_UNSTABLE
)

registerFPGAConfiguration(twin_scr7bug21107_score_nortos
  BITSTREAM /home/stand/users/aap-sc/bitstreams/bug_21107/1/22121300/top.bit
  OPENOCD_BOARD twin_scr7_x_1core_nortos
  TWIN_UNSTABLE
)
registerFPGAConfiguration(twin_scr7bug21107_score_rtoshw
  BITSTREAM /home/stand/users/aap-sc/bitstreams/bug_21107/1/22121300/top.bit
  OPENOCD_BOARD twin_scr7_x_1core_rtoshw
  TWIN_UNSTABLE
)

