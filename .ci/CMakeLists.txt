cmake_minimum_required(VERSION 3.22)

include(ExternalProject)
include(ProcessorCount)

project(OpenOCD)

ProcessorCount(CPU_COUNT)

set(DEPENDENCIES_LOCATION ${CMAKE_BINARY_DIR}/dependencies)

if (NOT DEPENDENCIES_DOWNLOADED)
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPENDENCIES_LOCATION})
  execute_process(
    COMMAND
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/fetch_dependencies.py
    TIMEOUT 60
    WORKING_DIRECTORY ${DEPENDENCIES_LOCATION}
    RESULT_VARIABLE DOWNLOAD_STATUS
  )

  if(DOWNLOAD_STATUS AND NOT DOWNLOAD_STATUS EQUAL 0)
    message(FATAL_ERROR "could not download dependencies: ${DOWNLOAD_STATUS}")
  endif()

  message("DOWNLOAD SUCCEDED")

  set(DEPENDENCIES_DOWNLOADED TRUE CACHE BOOL
    "Has dependencies been downloaded?" FORCE)
endif()


execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpmachine
                OUTPUT_VARIABLE CONFIGURE_HOST
                OUTPUT_STRIP_TRAILING_WHITESPACE)

include(cmake/DeclareDepsBuild.cmake)
set(DEPENDENCIES_INSTALL_PATH ${CMAKE_BINARY_DIR}/dependencies_install)
declare_build_dependencies(openocd_deps)

set(OPENOCD_INSTALL_PATH ${CMAKE_BINARY_DIR}/install_openocd/openocd)

# windows and linux have different set of features
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  include(cmake/OpenOCDConfigWindows.cmake)
else()
  include(cmake/OpenOCDConfigLinux.cmake)
endif()

get_filename_component(OPENOCD_SOURCES ${CMAKE_CURRENT_SOURCE_DIR} PATH)

ExternalProject_Add(openocd
  PREFIX OpenOCDBuild
  SOURCE_DIR ${OPENOCD_SOURCES}
  CONFIGURE_COMMAND
  # NOTE: we can't use PKG_CONFIG_LIBDIR because of udev dependency
  env
    PKG_CONFIG_PATH=${DEPENDENCIES_INSTALL_PATH}/lib/pkgconfig
    CC=${CMAKE_C_COMPILER}
      ${OPENOCD_SOURCES}/configure
        --prefix=${OPENOCD_INSTALL_PATH}
        --host=$<$<BOOL:${CMAKE_CROSSCOMPILING}>:${CONFIGURE_HOST}>
        ${OPENOCD_CONFIGURE_OPTIONS}
  # NOTE: it seems that cmake has a bug - it does not propagate the requested
  # number of cores, causing extreme number of threads to be created during
  # the build process
  BUILD_COMMAND make install -j${CPU_COUNT}
  INSTALL_COMMAND ${CMAKE_STRIP} --strip-all ${OPENOCD_INSTALL_PATH}/bin/openocd${CMAKE_EXECUTABLE_SUFFIX}
  INSTALL_COMMAND && mkdir ${OPENOCD_INSTALL_PATH}/share/openocd/scripts/syntacore -p
  INSTALL_COMMAND && ${OPENOCD_SOURCES}/tools/syntacore/ide_cloak.tcl
    ${OPENOCD_INSTALL_PATH}/share/openocd/scripts/syntacore/syntacore_ide_csr_cloak.cfg
  DEPENDS openocd_deps
)

get_filename_component(OPENOCD_INSTALL_PARENT ${OPENOCD_INSTALL_PATH} PATH)
add_custom_target(OpenOCDPackage
  WORKING_DIRECTORY ${OPENOCD_INSTALL_PATH}
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/utils/make_package.sh
          ${CMAKE_SYSTEM_NAME}
          ${OPENOCD_INSTALL_PARENT}
          ${DEPENDENCIES_INSTALL_PATH}
  DEPENDS openocd
)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  include(cmake/DeclareLinuxTests.cmake)
endif()

