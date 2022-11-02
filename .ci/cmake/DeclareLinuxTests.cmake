include(FetchContent)

set(FETCHCONTENT_BASE_DIR ToolChain)
set(FETCHCONTENT_QUIET FALSE)

set(NAS_USER $ENV{NAS_USER})
set(NAS_PASS $ENV{NAS_PASS})
set(SYNTACORE_NAS_SERVER $ENV{SYNTACORE_NAS_SERVER})

set(GCC_VERSION 2022.09-Ubuntu18-riscv-gcc-12.1.1-g02aeca3-220927T2007-g061b664)
set(SC_IDE_MD5 c82bcc0c1979b76a5daa86c3a3941bc4)
set(DISTRIB_PATH pub/Distrib/sc-ide/gcc)
set(SC_IDE_URL ftp://${NAS_USER}:${NAS_PASS}@${SYNTACORE_NAS_SERVER}/${DISTRIB_PATH}/${GCC_VERSION}.tar.gz)

if(DEFINED ENV{CUSTOM_IDE_URL})
  set(SC_IDE_URL $ENV{CUSTOM_IDE_URL})
endif()

FetchContent_Declare(sc-gcc
  URL ${SC_IDE_URL}
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(sc-gcc)

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

set(SPIKE_INSTALL_PATH ${CMAKE_BINARY_DIR}/install_spike)
ExternalProject_Add(spike
  PREFIX SpikeBuild
  SOURCE_DIR SpikeSources
  GIT_SHALLOW True
  GIT_REPOSITORY http://${NAS_USER}:${NAS_PASS}@gitlab.dev.syntacore.com/simulators/riscv-isa-sim
  GIT_TAG sc/main
  CONFIGURE_COMMAND
    ${CMAKE_BINARY_DIR}/SpikeSources/configure
    --prefix=${SPIKE_INSTALL_PATH}
)

configure_file(local_init.exp.in local_init.exp)

set(TGT_BOARD "")
if(DEFINED ENV{TARGET_BOARD})
  set(TGT_BOARD $ENV{TARGET_BOARD})
endif()

if ("${TGT_BOARD}" STREQUAL "")
  set(TARGET_BOARD_CMDLINE "")
  set(LOGS_DIR "SUMMARY")
else()
  set(TARGET_BOARD_CMDLINE "--target_board=${TGT_BOARD}")
  set(LOGS_DIR "SUMMARY_${TGT_BOARD}")
endif()

add_custom_target(test_dir ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory TestRun)
add_custom_target(OpenOCDTest
  WORKING_DIRECTORY TestRun
  COMMAND
    mkdir -p ${LOGS_DIR}
  COMMAND
        DEJAGNU=${OPENOCD_SOURCES}/testsuite/site.exp
    ${CMAKE_BINARY_DIR}/install_dejagnu/bin/runtest
        --src_dir=${OPENOCD_SOURCES}/testsuite
        ${TARGET_BOARD_CMDLINE}
        --tool=ocd
        --outdir=${LOGS_DIR}
        --local_init ${CMAKE_BINARY_DIR}/local_init.exp
  DEPENDS openocd dejagnu spike test_dir
)
