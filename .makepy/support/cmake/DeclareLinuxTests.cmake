if(NOT DEFINED RISCVGCC_DIR)
  message(FATAL_ERROR "RISCVGCC_DIR is not defined")
endif()
set(RISCV_GCC_BINARY ${RISCVGCC_DIR}/bin/riscv64-unknown-elf-gcc)

if(NOT DEFINED RISCVGDB_DIR)
  message(FATAL_ERROR "RISCVGDB_DIR is not defined")
endif()
set(RISCV_GDB_BINARY ${RISCVGDB_DIR}/rv64elf/bin/riscv64-unknown-elf-gdb)

if(NOT DEFINED RISCVSpike_DIR)
  message(FATAL_ERROR "RISCVSpike_DIR is not defined")
endif()
set(RISCV_SPIKE_SIM_BINARY ${RISCVSpike_DIR}/bin/spike)

if(NOT DEFINED RISCVTESTS_DIR)
  message(FATAL_ERROR "RISCVTESTS_DIR is not defined")
endif()

if (NOT DEFINED DEJAGNU_DIR)
  message(FATAL_ERROR "DEJAGNU_DIR is not defined")
endif()

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/dependencies_support/local_init.exp.in
  local_init.exp @ONLY)

# we expect that FpgaBoardInfo interface target is defined after this include
set(FPGA_SUPPORT_PROJECT_PATH ${OPENOCD_SOURCES}/testing/syntacore/fpga_support)
# this variable is used by BoardDefinitions.cmake
set(DEJAGNU_FPGA_BOARDS_DIRECTORY ${OPENOCD_SOURCES}/testing/dejagnu/boards)
include("${FPGA_SUPPORT_PROJECT_PATH}/cmake/BoardDefinitions.cmake")

add_custom_target("OpenOCDTestsOn_spike")
add_library(SpikeBoardInfo INTERFACE)

function(registerSpikeConfiguration SPIKE_CONFIGURATION_NAME)
  add_library(${SPIKE_CONFIGURATION_NAME} INTERFACE)
  set_property(TARGET ${SPIKE_CONFIGURATION_NAME}
    PROPERTY OPENOCD_BOARD ${SPIKE_CONFIGURATION_NAME}
  )

  set_property(TARGET SpikeBoardInfo
    PROPERTY ALL_SPIKE_CONFIGURATIONS ${SPIKE_CONFIGURATION_NAME}
    APPEND
  )
endfunction()

set(OPENOCD_TESTSUITE_DIRECTORY "${OPENOCD_SOURCES}/testsuite")
file(GLOB PATH_FOR_SPIKE_PLATFORMS
  ${OPENOCD_TESTSUITE_DIRECTORY}/boards/spike32*.exp
  ${OPENOCD_TESTSUITE_DIRECTORY}/boards/spike64*.exp
)

foreach(spike_platform_with_extension ${PATH_FOR_SPIKE_PLATFORMS})
  file(RELATIVE_PATH
    SPIKE_PLATFORMS
    "${OPENOCD_TESTSUITE_DIRECTORY}/boards/"
    ${spike_platform_with_extension}
  )
  string(REGEX REPLACE ".exp$" "" spike_platform ${SPIKE_PLATFORMS})
  registerSpikeConfiguration("${spike_platform}")
endforeach()

get_property(
  SPIKE_TEST_BOARDS
  TARGET SpikeBoardInfo
  PROPERTY ALL_SPIKE_CONFIGURATIONS)

get_property(
  FPGA_TEST_BOARDS
  TARGET FpgaBoardInfo
  PROPERTY ALL_CONFIGURATIONS)

set(TEST_BOARDS ${FPGA_TEST_BOARDS})
list(APPEND TEST_BOARDS ${SPIKE_TEST_BOARDS})

set(TESTING_ROOT "${CMAKE_BINARY_DIR}/testing")

set(DEJAGNU_TESTING_ROOT "${TESTING_ROOT}/dejagnu")
function(addNextToolToTestForBoard tool_name board_config_name)
  set(tool_dir "${DEJAGNU_TESTING_ROOT}/${board_config_name}/${tool_name}")
  set(tool_run_dir "${tool_dir}/runs")
  set(tool_summary_dir "${tool_dir}/SUMMARY")

  file(MAKE_DIRECTORY ${tool_run_dir} ${tool_summary_dir})

  get_property(
    openocd_board
    TARGET ${board_config}
    PROPERTY OPENOCD_BOARD)

  set(target_board_cmdline "--target_board=${openocd_board}")
  set(board_tests_target "OpenOCDTestsOn_${board_config}")
  set(board_tool_target "Tool_${tool_name}_For_${board_tests_target}")

  # somewhat dirty hack to identify if we should use default site.exp suitable
  # for spike runs, or an extended one suitable for fpga platforms
  # NOTE: techinically we can just use only the latter, but I want both
  # paths to be tested
  if (openocd_board MATCHES "^spike")
    set(GLOBAL_SITE_EXP ${OPENOCD_TESTSUITE_DIRECTORY}/site.exp)
  else()
    set(GLOBAL_SITE_EXP ${OPENOCD_SOURCES}/testing/dejagnu/site.exp)
  endif()

  # cmake-format: off
  add_custom_target(
    ${board_tool_target}
    WORKING_DIRECTORY ${tool_run_dir}
    COMMAND
      env DEJAGNU=${GLOBAL_SITE_EXP}
      ${DEJAGNU_DIR}/bin/runtest
        --src_dir=${OPENOCD_TESTSUITE_DIRECTORY}
        ${target_board_cmdline}
        --tool=${tool_name}
        --outdir=${tool_summary_dir}
        --local_init ${CMAKE_BINARY_DIR}/local_init.exp
    DEPENDS openocd)
  # cmake-format: on

  if(NOT TARGET ${board_tests_target})
    add_custom_target(${board_tests_target} DEPENDS ${board_tool_target})
    message(STATUS "Primary ${board_tests_target} defined")

    if(board_tests_target MATCHES "^OpenOCDTestsOn_spike")
      add_dependencies("OpenOCDTestsOn_spike" ${board_tests_target})
    endif()

  else()
    get_property(
      board_test_deps
      TARGET ${board_tests_target}
      PROPERTY TOOL_DEPENDENCIES)
    list(GET board_test_deps -1 last_added_tool)
    add_dependencies(${last_added_tool} ${board_tool_target})
    message(DEBUG "  ${last_added_tool} depends on ${board_tool_target}")
  endif()
  message(STATUS "   test target for tool testing ${board_tool_target} defined")
  set_property(
    TARGET ${board_tests_target}
    PROPERTY TOOL_DEPENDENCIES ${board_tool_target}
    APPEND)
endfunction()

function(addOpenOCDTestsForBoard board_config)
  # cmake-format: off
  addNextToolToTestForBoard(ocd ${board_config})
  addNextToolToTestForBoard(jtag ${board_config})
  addNextToolToTestForBoard(utils ${board_config})
  # cmake-format: on
endfunction()

foreach(test_board ${TEST_BOARDS})
  # cmake-format: off
  addOpenOCDTestsForBoard(${test_board})
  # cmake-format: on
endforeach()

find_package(
  Python
  COMPONENTS Interpreter
  REQUIRED)
set(RISCV_TESTS_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/RISCVTests)
ExternalProject_Add(
  riscv_tests
  SOURCE_DIR ${RISCV_TESTS_SOURCE_DIR}
  URL file://${RISCVTESTS_DIR}/riscv-tests
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND "")

set(RISCV_TESTS_RUN_DIR ${TESTING_ROOT}/riscv_tests)
set(RISCV_TESTS_LOGS_DIRNAME "${RISCV_TESTS_RUN_DIR}/logs")
add_custom_target(
  riscv_tests_run_dir ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${RISCV_TESTS_RUN_DIR}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${RISCV_TESTS_LOGS_DIRNAME}
  DEPENDS riscv_tests)

list(
  APPEND
  RISCV_TESTS_DEBUG_TARGETS_LIST
  spike32
  spike32-2
  spike32-2-hwthread
  spike64
  spike64-2
  spike64-2-hwthread)

add_custom_target(
  RISCVTestsDebug
  WORKING_DIRECTORY ${RISCV_TESTS_RUN_DIR}
  DEPENDS riscv_tests_run_dir)

function(add_riscv_test_debug_run_for_target target)
  set(target_name riscv_tests_debug_${target})
  set(wd_target_name ${target_name}_work_dir)
  set(wd_relative_path ${RISCV_TESTS_RUN_DIR}/WD_${target})

  add_custom_target(
    ${wd_target_name} ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${wd_relative_path}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${RISCV_TESTS_SOURCE_DIR}/debug/bin ${wd_relative_path}/bin
    DEPENDS riscv_tests_run_dir)

  # cmake-format: off
  add_custom_target(
    ${target_name}
    WORKING_DIRECTORY ${wd_relative_path}
    COMMAND
      env
        GCC=${RISCV_GCC_BINARY}
        GDB=${RISCV_GDB_BINARY}
        LOGS=${RISCV_TESTS_LOGS_DIRNAME}
        OCD=${OPENOCD_INSTALL_PATH}/bin/openocd
        ROOT=${RISCV_TESTS_SOURCE_DIR}/debug
        SIM=${RISCV_SPIKE_SIM_BINARY}
        TGT=${target}
      ${CMAKE_CURRENT_SOURCE_DIR}/dependencies_support/run-riscv-debug-tests.sh
    DEPENDS openocd ${wd_target_name})
  # cmake-format: on
  add_dependencies(RISCVTestsDebug ${target_name})
endfunction()

foreach(tgt ${RISCV_TESTS_DEBUG_TARGETS_LIST})
  add_riscv_test_debug_run_for_target(${tgt})
endforeach()
