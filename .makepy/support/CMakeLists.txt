cmake_minimum_required(VERSION 3.22)

include(ExternalProject)
include(ProcessorCount)

project(OpenOCD)

option(SC_OPENOCD_ENABLE_TESTS "Enable tests")

ProcessorCount(CPU_COUNT)

execute_process(
  COMMAND ${CMAKE_C_COMPILER} -dumpmachine
  OUTPUT_VARIABLE CONFIGURE_HOST
  OUTPUT_STRIP_TRAILING_WHITESPACE)

set(OPENOCD_TESTSUITE_INSTALL_PATH ${CMAKE_BINARY_DIR}/install_testsuite)
set(OPENOCD_INSTALL_PATH ${CMAKE_BINARY_DIR}/install_openocd/openocd)

# windows and linux have different set of features
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  include(cmake/OpenOCDConfigWindows.cmake)
else()
  include(cmake/OpenOCDConfigLinux.cmake)
endif()

get_filename_component(OPENOCD_SOURCES ${CMAKE_SOURCE_DIR}/../.. ABSOLUTE)

# cmake-format: off
set(OPENOCD_EXTRA_CFLAGS "")

option(SC_OPENOCD_ENABLE_SANITIZERS "Enable sanitizers")
if (SC_OPENOCD_ENABLE_SANITIZERS)
  #set(OPENOCD_EXTRA_CFLAGS
  #  "${OPENOCD_EXTRA_CFLAGS} -fsanitize=address,undefined -Wl,-ldl")
  set(OPENOCD_EXTRA_CFLAGS
    "${OPENOCD_EXTRA_CFLAGS} -fsanitize=undefined -Wl,-ldl")
  if (SC_OPENOCD_STRICT_SANITIZERS)
    set(OPENOCD_EXTRA_CFLAGS "${OPENOCD_EXTRA_CFLAGS} -fno-sanitize-recover")
  endif()
endif()

set(OPENOCD_CFLAGS "$<$<CONFIG:Release>:-O2> $<$<CONFIG:Debug>:-O0 -g> $<$<BOOL:${CMAKE_SYSROOT}>:--sysroot=${CMAKE_SYSROOT}> ${OPENOCD_EXTRA_CFLAGS}")
ExternalProject_Add(
  openocd
  PREFIX OpenOCDBuild
  SOURCE_DIR ${OPENOCD_SOURCES}
  BUILD_ALWAYS TRUE
  CONFIGURE_COMMAND
    # NOTE: we can't use PKG_CONFIG_LIBDIR because of udev dependency: when we
    # set PKG_CONFIG_LIBDIR libraries are searched only at that directory.
    # Since in modern Linux distributions udev is part of systemd - this would
    # require us to have the whole systemd distribution at that location. This
    # is an overkill. So we link with udev dynamically using the library found
    # in the build environment.
    env
      PKG_CONFIG_PATH=$ENV{PKG_CONFIG_PATH}
      CC=${CMAKE_C_COMPILER}
      CFLAGS=${OPENOCD_CFLAGS}
    ${OPENOCD_SOURCES}/configure
      --prefix=${OPENOCD_INSTALL_PATH}
      --host=$<$<BOOL:${CMAKE_CROSSCOMPILING}>:${CONFIGURE_HOST}>
      ${OPENOCD_CONFIGURE_OPTIONS}
  BUILD_COMMAND make $<IF:$<CONFIG:DEBUG>,install,install-strip> -j${CPU_COUNT})
# cmake-format: on

set(TESTSUITE_NAME ocd_transferable_testsuite)
add_custom_target(
  transferable_testsuite
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/utils/make_distr.sh
          ${CMAKE_BINARY_DIR}
          ${OPENOCD_TESTSUITE_INSTALL_PATH}
          ${TESTSUITE_NAME}
  DEPENDS openocd
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SC_OPENOCD_ENABLE_TESTS)
  include(cmake/DeclareLinuxTests.cmake)
endif()
