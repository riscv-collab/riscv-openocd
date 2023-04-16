cmake_minimum_required(VERSION 3.16)

project(FpgaSupport)

set(SYNTACORE_NAS_SERVER $ENV{SYNTACORE_NAS_SERVER})
set(ARTY_BITSTREAM_FTP_STORAGE ${SYNTACORE_NAS_SERVER}/pub/exchange/ot-sc/arty-a7-100t)

# NOTE: well... it's better to specify this externally, but at the momement
# I'm too lazy for that
get_filename_component(OPENOCD_SRC_DIRECTORY ../../ ABSOLUTE)
cmake_path(APPEND OPENOCD_SRC_DIRECTORY testsuite
  OUTPUT_VARIABLE OPENOCD_TESTSUITE_DIRECTORY)
include(cmake/BoardDefinitions.cmake)
printAvailableFPGAConfigurations()

add_custom_target(fpga_configuration_registry)
function(generate_configuration_registry registry_name configs)
  add_custom_command(
    OUTPUT ${registry_name}
    COMMAND ${CMAKE_COMMAND} -E echo ${configs} >${registry_name}
    COMMAND sed 's/ /\\n/g' -i ${registry_name}
  )
  set(REG_TARGET registry-${registry_name})
  add_custom_target(${REG_TARGET}
    DEPENDS ${registry_name}
  )
  add_dependencies(fpga_configuration_registry ${REG_TARGET})
endfunction()

set(TESTING_CYCLES
  ZALMAN_NIGHTLY_CONFIGURATIONS
  TWIN_NIGHTLY_CONFIGURATIONS
  TWIN_SCR9_CONFIGURATIONS
  TWIN_UNSTABLE_CONFIGURATIONS
)
foreach(testing_cycle ${TESTING_CYCLES})
  get_property(configurations TARGET FpgaBoardInfo PROPERTY ${testing_cycle})
  generate_configuration_registry(${testing_cycle}.list "${configurations}")
endforeach()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/BoardInit)

get_property(AVAILABLE_FPGA_CONFIGURATIONS
            TARGET FpgaBoardInfo PROPERTY ALL_CONFIGURATIONS)
foreach(fpga_config ${AVAILABLE_FPGA_CONFIGURATIONS})
  get_property(bitstream TARGET ${fpga_config} PROPERTY BITSTREAM)
  get_property(use_arty_tool TARGET ${fpga_config} PROPERTY USE_ARTY_TOOLS)

  set(BOARD_INIT_TARGET init_${fpga_config})
  if (use_arty_tool)
    add_custom_target(${BOARD_INIT_TARGET}
      WORKING_DIRECTORY BoardInit
      COMMAND
        env BITSTREAM_FTP_STORAGE=${ARTY_BITSTREAM_FTP_STORAGE}
          ${CMAKE_CURRENT_SOURCE_DIR}/scripts/fetch_arty_bitstream.sh
          ${bitstream}
      COMMAND
          ${CMAKE_CURRENT_SOURCE_DIR}/scripts/arty_inst.exp
          ${bitstream}.bitstream.bit
    )
  else()
    add_custom_target(${BOARD_INIT_TARGET}
      WORKING_DIRECTORY /home/stand/fpga-runner-stand
      COMMAND
          ${CMAKE_CURRENT_SOURCE_DIR}/scripts/runner_inst.exp
          twin ${bitstream}
    )
  endif()
endforeach()
