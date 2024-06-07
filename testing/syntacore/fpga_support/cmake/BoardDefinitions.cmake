set(SYNTACORE_NAS_SERVER nas.dev.syntacore.com)
set(ARTY_BITSTREAM_FTP_STORAGE ${SYNTACORE_NAS_SERVER}/pub/exchange/ot-sc/arty-a7-100t)

message(STATUS "Syntacore NAS server: ${SYNTACORE_NAS_SERVER}")

if (NOT DEJAGNU_FPGA_BOARDS_DIRECTORY)
  message(FATAL_ERROR "DEJAGNU_FPGA_BOARDS_DIRECTORY is not specified")
endif()

message(STATUS "DEJAGNU_FPGA_BOARDS_DIRECTORY: ${DEJAGNU_FPGA_BOARDS_DIRECTORY}")

add_library(FpgaBoardInfo INTERFACE)

function(ensureFileExists NAME)
  foreach(dir ${ARGN})
    cmake_path(APPEND dir ${NAME} OUTPUT_VARIABLE TEST_PATH)
    if (EXISTS ${TEST_PATH})
      return()
    endif()
  endforeach()
  message(FATAL_ERROR "could not find file ${NAME} at ${ARGN}")
endfunction()

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

  cmake_path(APPEND DEJAGNU_FPGA_BOARDS_DIRECTORY embargo OUTPUT_VARIABLE EMBARGO_BOARDS)
  cmake_path(APPEND DEJAGNU_FPGA_BOARDS_DIRECTORY syntacore OUTPUT_VARIABLE EXAMPLE_BOARDS)
  ensureFileExists("${BOARD_ARG_OPENOCD_BOARD}.exp" ${EMBARGO_BOARDS} ${EXAMPLE_BOARDS})

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

function(addSCR7X2_L2Config RELEASE_NAME BITSTREAM)
  set(options TWIN_NIGHTLY)
  cmake_parse_arguments(PARSE_ARGV 1 SCR7BOARD_ARG
    "${options}" "" ""
  )

  if (SCR7BOARD_ARG_TWIN_NIGHTLY)
    set(TESTING_CYCLE TWIN_NIGHTLY)
  else()
    set(TESTING_CYCLE TWIN_UNSTABLE)
  endif()

  set(CONFIG_RVV_MCORE2_NORTOS "twin_rvv_${RELEASE_NAME}_mcore2_nortos")
  set(CONFIG_MCORE2_NORTOS "twin_${RELEASE_NAME}_mcore2_nortos")
  set(CONFIG_MCORE2_RTOSHW "twin_${RELEASE_NAME}_mcore2_rtoshw")
  set(CONFIG_SMP2 "twin_${RELEASE_NAME}_smp2")

  registerFPGAConfiguration(${CONFIG_RVV_MCORE2_NORTOS}
    BITSTREAM ${BITSTREAM}
    OPENOCD_BOARD scr7rvv_x_mcore2_nortos
    ${TESTING_CYCLE}
  )
  registerFPGAConfiguration(${CONFIG_MCORE2_NORTOS}
    BITSTREAM ${BITSTREAM}
    OPENOCD_BOARD scr7_x_mcore2_nortos
    ${TESTING_CYCLE}
  )
  registerFPGAConfiguration(${CONFIG_MCORE2_RTOSHW}
    BITSTREAM ${BITSTREAM}
    OPENOCD_BOARD scr7_x_mcore2_rtoshw
    ${TESTING_CYCLE}
  )
  registerFPGAConfiguration(${CONFIG_SMP2}
    BITSTREAM ${BITSTREAM}
    OPENOCD_BOARD scr7_x_smp2
    ${TESTING_CYCLE}
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

  registerFPGAConfiguration("twin_norvv_${RELEASE_NAME}_score_nortos"
    BITSTREAM ${BITSTREAM}
    OPENOCD_BOARD scr7norvv_x_1core_nortos
    ${TESTING_CYCLE}
  )
  registerFPGAConfiguration("twin_norvv_${RELEASE_NAME}_score_rtoshw"
    BITSTREAM ${BITSTREAM}
    OPENOCD_BOARD scr7norvv_x_1core_rtoshw
    ${TESTING_CYCLE}
  )
  registerFPGAConfiguration("twin_rvv_${RELEASE_NAME}_score_nortos"
    BITSTREAM ${BITSTREAM}
    OPENOCD_BOARD scr7_x_1core_nortos
    ${TESTING_CYCLE}
  )
endfunction()

function(addSCR9L2Config RELEASE_NAME BITSTREAM)

  set(options TWIN_NIGHTLY)
  cmake_parse_arguments(PARSE_ARGV 1 SCR9BOARD_ARG
    "${options}" "" ""
  )

  if (SCR9BOARD_ARG_TWIN_NIGHTLY)
    set(TESTING_CYCLE TWIN_NIGHTLY)
  else()
    set(TESTING_CYCLE TWIN_UNSTABLE)
  endif()

  registerFPGAConfiguration("twin_norvv_${RELEASE_NAME}_score_nortos"
    BITSTREAM ${BITSTREAM}
    OPENOCD_BOARD scr9norvv_x_1core_nortos
    ${TESTING_CYCLE}
  )
  registerFPGAConfiguration("twin_norvv_${RELEASE_NAME}_score_rtoshw"
    BITSTREAM ${BITSTREAM}
    OPENOCD_BOARD scr9norvv_x_1core_rtoshw
    ${TESTING_CYCLE}
  )
  registerFPGAConfiguration("twin_rvv_${RELEASE_NAME}_score_nortos"
    BITSTREAM ${BITSTREAM}
    OPENOCD_BOARD scr9_x_1core_nortos
    ${TESTING_CYCLE}
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
  OPENOCD_BOARD scr1_32
  ARTY_STAND_TOOL
  ZALMAN_NIGHTLY
)
registerFPGAConfiguration(arty100_scr3_32
  BITSTREAM_FROM_NAS
  BITSTREAM rv32_single_scr3_tcm_ipic_hiperf.rar
  OPENOCD_BOARD scr3_32
  ARTY_STAND_TOOL
  ZALMAN_NIGHTLY
)
registerFPGAConfiguration(arty100_scr4_32_imcaf
  BITSTREAM_FROM_NAS
  BITSTREAM rv32_single_scr4_tcm_l1_ipic_hiperf_256MB_wo_D.tar.gz
  OPENOCD_BOARD scr4_32_imcaf
  ARTY_STAND_TOOL
  ZALMAN_NIGHTLY
)
registerFPGAConfiguration(arty100_scr4_32_imcafd
  BITSTREAM_FROM_NAS
  BITSTREAM rv32_single_scr4_tcm_l1_ipic_hiperf_256MB.tar.gz
  OPENOCD_BOARD scr4_32_imcafd
  ARTY_STAND_TOOL
  ZALMAN_NIGHTLY
)

# Twin stands
registerFPGAConfiguration(twin_scr5_32
  BITSTREAM remote:/home/stand/users/aap-sc/BITSTREAMS/rv32_cluster_single_scr5_l1_l2_plic_hiperf/vcu118_top_new.bit
  OPENOCD_BOARD scr5_32
  TWIN_NIGHTLY
)
registerFPGAConfiguration(twin_scr5_64
  BITSTREAM remote:/home/stand/users/aap-sc/BITSTREAMS/rv64_cluster_single_scr5_tcm_l1_l2_plic_hiperf/vcu118_top_new.bit
  OPENOCD_BOARD scr5_64
  TWIN_NIGHTLY
)
registerFPGAConfiguration(twin_scr6
  BITSTREAM remote:/home/stand/users/aap-sc/BITSTREAMS/rv64_cluster_single_scr6_eval/vcu118_top_new.bit
  OPENOCD_BOARD scr6
  TWIN_NIGHTLY
)

addSCR7X2_L2Config(scr7_l2_23ww46.4.0
  remote:/home/stand/users/aap-sc/BITSTREAMS/scr7_l2_23ww46.4.0/scr7_l2_23ww46.4.0.bit
  TWIN_NIGHTLY
)
addSCR7L2Config(scr7_l2_24ww13.5.0
  remote:/home/stand/users/aap-sc/BITSTREAMS/scr7_l2_24ww13.5.0/scr7_l2_24ww13.5.0.bit
  TWIN_NIGHTLY
)
addSCR7L2Config(scr7_l2_24ww20.4.0
  remote:/home/stand/users/aap-sc/BITSTREAMS/scr7_l2_24ww20.4.0/scr7_l2_24ww20.4.0.bit
  TWIN_NIGHTLY
)
addSCR7L2Config(scr7_dev
  remote:/home/stand/users/aap-sc/BITSTREAMS/scr7_l2_23ww46.4.0/scr7_l2_23ww46.4.0.bit
)

# SCR9 testing
addSCR9L2Config(scr9_l2_23ww45.4.0
  remote:/home/stand/users/aap-sc/BITSTREAMS/scr9_l2_23ww45.4.0/scr9_l2_23ww45.4.0.bit
  TWIN_NIGHTLY
)
addSCR9L2Config(scr9_l2_24ww13.5.0
  remote:/home/stand/users/aap-sc/BITSTREAMS/scr9_l2_24ww13.5.0/scr9_l2_24ww13.5.0.bit
  TWIN_NIGHTLY
)
addSCR9L2Config(scr9_l2_24ww21.1.0
  remote:/home/stand/users/aap-sc/BITSTREAMS/scr9_l2_24ww21.1.0/scr9_l2_24ww21.1.0.bit
  TWIN_NIGHTLY
)
addSCR9L2Config(scr9_dev
  remote:/home/stand/users/aap-sc/BITSTREAMS/scr9_l2_23ww45.4.0/scr9_l2_23ww45.4.0.bit
)
