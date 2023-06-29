cmake_minimum_required(VERSION 3.22)

include(ExternalProject)
include(ProcessorCount)

project(OpenOCD)

ProcessorCount(CPU_COUNT)

execute_process(
  COMMAND ${CMAKE_C_COMPILER} -dumpmachine
  OUTPUT_VARIABLE CONFIGURE_HOST
  OUTPUT_STRIP_TRAILING_WHITESPACE)

set(DEPENDENCIES_LOCATION ${CMAKE_BINARY_DIR}/external_dependencies/sources)
set(DEPENDENCIES_INSTALL_PATH ${CMAKE_BINARY_DIR}/dependencies_install)
include(cmake/DeclareDepsBuild.cmake)
declare_build_dependencies(openocd_deps)

set(OPENOCD_INSTALL_PATH ${CMAKE_BINARY_DIR}/install_openocd/openocd)

# windows and linux have different set of features
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  include(cmake/OpenOCDConfigWindows.cmake)
else()
  include(cmake/OpenOCDConfigLinux.cmake)
endif()

get_filename_component(OPENOCD_SOURCES ${CMAKE_SOURCE_DIR}/../.. ABSOLUTE)

# cmake-format: off
set(OCD_PKG_CONFIG_LIB ${DEPENDENCIES_INSTALL_PATH}/lib/pkgconfig)
set(OCD_PKG_CONFIG_LIB64 ${DEPENDENCIES_INSTALL_PATH}/lib64/pkgconfig)

set(OPENOCD_SCRIPTS_INSTALL_DIR
  ${OPENOCD_INSTALL_PATH}/share/openocd/scripts/syntacore)

ExternalProject_Add(
  openocd
  PREFIX OpenOCDBuild
  SOURCE_DIR ${OPENOCD_SOURCES}
  BUILD_ALWAYS TRUE
  CONFIGURE_COMMAND
    # NOTE: we can't use PKG_CONFIG_LIBDIR because of udev dependency: when we
    # set PKG_CONFIG_LIBDIR libraries are searched only at that directory.
    # Since in modern linux distributions udev is part of systemd - this will
    # require us to have the whole systemd distribution at that location. This
    # is an overkill. So we choose to link dynamically with whatever library we
    # have in the build environment.
    env
      PKG_CONFIG_PATH=${OCD_PKG_CONFIG_LIB}:${OCD_PKG_CONFIG_LIB64}
      CC=${CMAKE_C_COMPILER}
    ${OPENOCD_SOURCES}/configure
      --prefix=${OPENOCD_INSTALL_PATH}
      --host=$<$<BOOL:${CMAKE_CROSSCOMPILING}>:${CONFIGURE_HOST}>
      ${OPENOCD_CONFIGURE_OPTIONS}
  # NOTE: it seems that cmake has a bug - it does not propagate the requested
  # number of cores, causing extreme number of threads to be created during the
  # build process
  BUILD_COMMAND make install -j${CPU_COUNT}
  INSTALL_COMMAND
    ${CMAKE_STRIP} --strip-all
      ${OPENOCD_INSTALL_PATH}/bin/openocd${CMAKE_EXECUTABLE_SUFFIX} &&
    mkdir -p -- ${OPENOCD_SCRIPTS_INSTALL_DIR} &&
    cp ${OPENOCD_SOURCES}/tcl/tools/syntacore/sc_fpga_lib.tcl
       ${OPENOCD_SCRIPTS_INSTALL_DIR}/ &&
    cp ${OPENOCD_SOURCES}/tools/syntacore/openocd_run.exp
       ${OPENOCD_INSTALL_PATH}/bin &&
    ${OPENOCD_SOURCES}/tools/syntacore/ide_cloak.tcl
      ${OPENOCD_SCRIPTS_INSTALL_DIR}/syntacore_ide_csr_cloak.cfg
  DEPENDS openocd_deps)
# cmake-format: on

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  include(cmake/DeclareLinuxTests.cmake)
endif()
