include(FetchContent)

set(FETCHCONTENT_BASE_DIR ToolChain)
set(FETCHCONTENT_QUIET FALSE)

set(DT_STORAGE "http://artifactory.dev.syntacore.com:8082/artifactory/tools-gitlab-artifacts")
set(GDB_URL "${DT_STORAGE}/riscv-binutils-gdb/197d5a51/x86_Lin-x86_Lin-RISCV64_Elf_binutils-gdb.tar.gz")
set(GCC_URL "${DT_STORAGE}/riscv-gcc/d71188d82/linux_gcc.tar.gz")
set(SPIKE_URL "${DT_STORAGE}/spike/sc_main/230424-213636_e65e8816/spike-23_04_24-x86_64-ubuntu-18.04-e65e881676a6.tar.gz")

FetchContent_Declare(sc-gcc
  URL ${GCC_URL}
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_Declare(sc-gdb
  URL ${GDB_URL}
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_Declare(sc-spike
  URL ${SPIKE_URL}
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(sc-gcc)
FetchContent_MakeAvailable(sc-gdb)
FetchContent_MakeAvailable(sc-spike)

set(DEJAGNU_SRC_CODE    dejagnu-1.6.3.tar.gz)

ExternalProject_Add(dejagnu
  PREFIX DejaGnuBuild
  SOURCE_DIR DejaGNUSources
  URL file://${DEPENDENCIES_LOCATION}/${DEJAGNU_SRC_CODE}
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  CONFIGURE_COMMAND
    ${CMAKE_BINARY_DIR}/DejaGNUSources/configure
    --prefix=${CMAKE_BINARY_DIR}/install_dejagnu
)

configure_file(local_init.exp.in local_init.exp)

set(TGT_BOARD "")
if(DEFINED ENV{TARGET_BOARD})
  set(TGT_BOARD $ENV{TARGET_BOARD})
endif()

set(TEST_RUN_DIR "${CMAKE_BINARY_DIR}/TestRun")
if ("${TGT_BOARD}" STREQUAL "")
  set(TARGET_BOARD_CMDLINE "")
  set(TEST_WORKING_DIR "${TEST_RUN_DIR}/runs")
  set(TEST_SUMMARY_DIR "${TEST_RUN_DIR}/SUMMARY")
else()
  set(TARGET_BOARD_CMDLINE "--target_board=${TGT_BOARD}")
  set(TEST_WORKING_DIR "${TEST_RUN_DIR}/run_${TGT_BOARD}")
  set(TEST_SUMMARY_DIR "${TEST_RUN_DIR}/SUMMARY_${TGT_BOARD}")
endif()
file(MAKE_DIRECTORY ${TEST_WORKING_DIR} ${TEST_SUMMARY_DIR})

add_custom_target(OpenOCDTest
  WORKING_DIRECTORY ${TEST_WORKING_DIR}
  COMMAND
    env DEJAGNU=${OPENOCD_SOURCES}/testsuite/site.exp
    ${CMAKE_BINARY_DIR}/install_dejagnu/bin/runtest
        --src_dir=${OPENOCD_SOURCES}/testsuite
        ${TARGET_BOARD_CMDLINE}
        --tool=ocd
        --outdir=${TEST_SUMMARY_DIR}
        --local_init ${CMAKE_BINARY_DIR}/local_init.exp
  DEPENDS openocd dejagnu
)

set(BUILD_ID $ENV{BUILD_ID})
set(STAND_ID $ENV{STAND_ID})
set(ARTIFACTORY_KEY $ENV{ARTIFACTORY_API_KEY})
add_custom_target(OpenOCDTestSuccess
  COMMAND
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/report_test_success.sh
    ${TEST_RUN_DIR}
    ${STAND_ID}
    ${ARTIFACTORY_KEY}
)

add_custom_target(OpenOCDTestReport
  COMMAND
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/upload_testing_results.sh
    ${TEST_RUN_DIR}
    ${BUILD_ID}
    ${ARTIFACTORY_KEY}
)

find_package(Python COMPONENTS Interpreter REQUIRED)
# TODO: move this to manifest
set(RISCV_TESTS_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/RISCVTests)
ExternalProject_Add(riscv_tests
  SOURCE_DIR ${RISCV_TESTS_SOURCE_DIR}
  GIT_REPOSITORY https://github.com/riscv-software-src/riscv-tests.git
  GIT_TAG 48491dadafcd59442c5bf22603fb0a7f1c589cb9
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  PATCH_COMMAND
        git checkout debug/targets/RISC-V
    &&  git apply ${CMAKE_CURRENT_SOURCE_DIR}/riscv_tests_patches/memory_sample_thresholds.patch
)

set(RISCV_TESTS_RUN_DIR ${CMAKE_CURRENT_BINARY_DIR}/RISCVTestsRun)
set(RISCV_TESTS_LOGS_DIRNAME "${RISCV_TESTS_RUN_DIR}/logs")
add_custom_target(riscv_tests_run_dir ALL
  COMMAND
    ${CMAKE_COMMAND} -E make_directory ${RISCV_TESTS_RUN_DIR}
  COMMAND
    ${CMAKE_COMMAND} -E make_directory ${RISCV_TESTS_LOGS_DIRNAME}
  DEPENDS riscv_tests
)

list(APPEND RISCV_TESTS_DEBUG_TARGETS_LIST
  spike32
  spike32-2
  spike32-2-hwthread
  spike64
  spike64-2
  spike64-2-hwthread
)

add_custom_target(RISCVTestsDebug
  WORKING_DIRECTORY ${RISCV_TESTS_RUN_DIR}
  DEPENDS riscv_tests_run_dir
)
function(add_riscv_test_debug_run_for_target target)
  set(TARGET_NAME riscv_tests_debug_${target})
  set(WD_TARGET_NAME ${TARGET_NAME}_work_dir)
  set(WD_RELATIVE_PATH ${RISCV_TESTS_RUN_DIR}/WD_${target})

  add_custom_target(${WD_TARGET_NAME} ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${WD_RELATIVE_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${RISCV_TESTS_SOURCE_DIR}/debug/bin
            ${WD_RELATIVE_PATH}/bin
    DEPENDS riscv_tests_run_dir)

  add_custom_target(${TARGET_NAME}
    WORKING_DIRECTORY ${WD_RELATIVE_PATH}
    COMMAND
      env LOGS=${RISCV_TESTS_LOGS_DIRNAME}
          GCC=${sc-gcc_SOURCE_DIR}/bin/riscv64-unknown-elf-gcc
          GDB=${sc-gdb_SOURCE_DIR}/bin/riscv64-unknown-elf-gdb
          SIM=${sc-spike_SOURCE_DIR}/spike/bin/spike
          OCD=${OPENOCD_INSTALL_PATH}/bin/openocd
          ROOT=${RISCV_TESTS_SOURCE_DIR}/debug
          TGT=${target}
      ${CMAKE_CURRENT_SOURCE_DIR}/utils/run-riscv-debug-tests.sh
      DEPENDS openocd ${WD_TARGET_NAME}
  )
  add_dependencies(RISCVTestsDebug ${TARGET_NAME})
endfunction()

foreach(tgt ${RISCV_TESTS_DEBUG_TARGETS_LIST})
  add_riscv_test_debug_run_for_target(${tgt})
endforeach()
