cmake_minimum_required(VERSION 3.22)

include(ExternalProject)
include(FetchContent)

project(OpenOCD_CI)

set(FETCHCONTENT_BASE_DIR ToolChain)
set(FETCHCONTENT_QUIET FALSE)

set(NAS_USER $ENV{NAS_USER})
set(NAS_PASS $ENV{NAS_PASS})

set(GCC_VERSION riscv-gcc-12.1.1-g02aeca3-220922T1838-g4bf3bb1)
set(DISTRIB_PATH pub/Distrib/sc-ide/ci_artifacts/IDE/22_09_22_370)
FetchContent_Declare(sc-gcc
  URL ftp://${NAS_USER}:${NAS_PASS}@192.168.1.100/${DISTRIB_PATH}/${GCC_VERSION}.tar.gz
)
FetchContent_MakeAvailable(sc-gcc)

ExternalProject_Add(dejagnu
  PREFIX DejaGnuBuild
  SOURCE_DIR DejaGNUSources
  URL https://ftp.gnu.org/gnu/dejagnu/dejagnu-1.6.3.tar.gz
  CONFIGURE_COMMAND
    ${CMAKE_BINARY_DIR}/DejaGNUSources/configure
    --prefix=${CMAKE_BINARY_DIR}/install_dejagnu
)

ExternalProject_Add(spike
  PREFIX SpikeBuild
  SOURCE_DIR SpikeSources
  GIT_SHALLOW True
  GIT_REPOSITORY http://${NAS_USER}:${NAS_PASS}@gitlab.dev.syntacore.com/ar-sc/riscv-isa-sim
  GIT_TAG sc/main
  CONFIGURE_COMMAND
    ${CMAKE_BINARY_DIR}/SpikeSources/configure
    --prefix=${CMAKE_BINARY_DIR}/install_spike
)

get_filename_component(OPENOCD_SOURCES ${CMAKE_CURRENT_SOURCE_DIR} PATH)
ExternalProject_Add(openocd
  PREFIX OpenOCDBuild
  SOURCE_DIR ${OPENOCD_SOURCES}
  CONFIGURE_COMMAND
    ${OPENOCD_SOURCES}/configure
    --prefix=${CMAKE_BINARY_DIR}/install_openocd
    --enable-rshim
    --enable-ftdi
    --enable-ftdi-oscan1
    --enable-stlink
    --enable-ti-icdi
    --enable-ulink
    --enable-usb-blaster-2
    --enable-ft232r
    --enable-vsllink
    --enable-xds110
    --enable-cmsis-dap-v2
    --enable-osbdm
    --enable-opendous
    --enable-armjtagew
    --enable-rlink
    --enable-usbprog
    --enable-cmsis-dap
    --enable-nulink
    --enable-kitprog
    --enable-usb-blaster
    --enable-presto
    --enable-openjtag
    --enable-buspirate
    --enable-jlink
    --enable-aice
    --enable-parport
    --disable-parport-ppdev
    --enable-jtag_vpi
    --enable-vdebug
    --enable-jtag_dpi
    --enable-amtjtagaccel
    --enable-bcm2835gpio
    --enable-imx_gpio
    --enable-ep93xx
    --enable-at91rm9200
    --enable-gw16012
    --enable-sysfsgpio
    --enable-xlnx-pcie-xvc
    --enable-remote-bitbang
    --enable-sysfsgpio
    # Capstone is disabled, since 0.11 has the support only for arm and aarch64
    #--with-capstone
)

add_custom_target(test_dir ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory TestRun)
add_custom_target(OpenOCDTest
  WORKING_DIRECTORY TestRun
  COMMAND
    env PATH="${CMAKE_BINARY_DIR}/ToolChain/sc-gcc-src/bin:${CMAKE_BINARY_DIR}/install_spike/bin:$ENV{PATH}"
        OPENOCD_ROOT=${CMAKE_BINARY_DIR}/install_openocd
        DEJAGNU=${OPENOCD_SOURCES}/testsuite/site.exp
    ${CMAKE_BINARY_DIR}/install_dejagnu/bin/runtest
        --src_dir=${OPENOCD_SOURCES}/testsuite
        --tool=ocd
  DEPENDS openocd dejagnu spike test_dir
)
