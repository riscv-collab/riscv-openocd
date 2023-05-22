find_package(riscv-gcc REQUIRED)

set(RISCVGCC_DIR "${riscv-gcc_PACKAGE_FOLDER_RELEASE}")
set(DEJAGNU_SRC_CODE dejagnu)

# cmake-format: off
ExternalProject_Add(
  dejagnu
  PREFIX DejaGnuBuild
  SOURCE_DIR DejaGNUSources
  URL file://${DEPENDENCIES_LOCATION}/${DEJAGNU_SRC_CODE}
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  CONFIGURE_COMMAND
    ${CMAKE_BINARY_DIR}/DejaGNUSources/configure
      --prefix=${CMAKE_BINARY_DIR}/install_dejagnu)
# cmake-format: on

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/dependencies_support/local_init.exp.in
  local_init.exp @ONLY)

# this variable is used by BoardDefinitions.cmake
set(OPENOCD_TESTSUITE_DIRECTORY "${OPENOCD_SOURCES}/testsuite")
# we expect that FpgaBoardInfo interface target is defined after this include
set(FPGA_SUPPORT_PROJECT_PATH ${OPENOCD_SOURCES}/testing/syntacore/fpga_support)
include("${FPGA_SUPPORT_PROJECT_PATH}/cmake/BoardDefinitions.cmake")
get_property(
  FPGA_TEST_BOARDS
  TARGET FpgaBoardInfo
  PROPERTY ALL_CONFIGURATIONS)
set(TEST_BOARDS spike ${FPGA_TEST_BOARDS})

set(TESTING_ROOT "${CMAKE_BINARY_DIR}/TestRun")
function(addOpenOCDTestsForBoard board_config)
  set(test_run_dir "${TESTING_ROOT}/${board_config}")
  # TODO: implement separate working/summary dir for each tool
  set(test_working_dir "${test_run_dir}/runs")
  set(test_summary_dir "${test_run_dir}/SUMMARY")
  file(MAKE_DIRECTORY ${test_working_dir} ${test_summary_dir})

  if(board_config STREQUAL "spike")
    set(target_board_cmdline "")
  else()
    get_property(
      openocd_board
      TARGET ${board_config}
      PROPERTY OPENOCD_BOARD)
    set(target_board_cmdline "--target_board=${openocd_board}")
  endif()

  set(test_target_name "OpenOCDTestsOn_${board_config}")
  # cmake-format: off
  add_custom_target(
    ${test_target_name}
    WORKING_DIRECTORY ${test_working_dir}
    COMMAND
      env DEJAGNU=${OPENOCD_TESTSUITE_DIRECTORY}/site.exp
      ${CMAKE_BINARY_DIR}/install_dejagnu/bin/runtest
        --src_dir=${OPENOCD_TESTSUITE_DIRECTORY}
        ${target_board_cmdline}
        --tool=ocd
        --outdir=${test_summary_dir}
        --local_init ${CMAKE_BINARY_DIR}/local_init.exp
    DEPENDS openocd dejagnu)
  # cmake-format: on
  message(STATUS "test target ${test_target_name} defined")
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
  URL file://${DEPENDENCIES_LOCATION}/riscv_tests
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND "")

set(RISCV_TESTS_RUN_DIR ${CMAKE_CURRENT_BINARY_DIR}/RISCVTestsRun)
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
        LOGS=${RISCV_TESTS_LOGS_DIRNAME}
        GCC=${RISCVGCC_DIR}/bin/riscv64-unknown-elf-gcc
        GDB=${RISCVGDB_DIR}/bin/riscv64-unknown-elf-gdb
        SIM=${RISCVSpike_DIR}/bin/spike
        OCD=${OPENOCD_INSTALL_PATH}/bin/openocd
        ROOT=${RISCV_TESTS_SOURCE_DIR}/debug
        TGT=${target}
      ${CMAKE_CURRENT_SOURCE_DIR}/dependencies_support/run-riscv-debug-tests.sh
    DEPENDS openocd ${wd_target_name})
  # cmake-format: on
  add_dependencies(RISCVTestsDebug ${target_name})
endfunction()

foreach(tgt ${RISCV_TESTS_DEBUG_TARGETS_LIST})
  add_riscv_test_debug_run_for_target(${tgt})
endforeach()
