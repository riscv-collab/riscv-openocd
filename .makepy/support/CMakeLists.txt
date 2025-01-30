cmake_minimum_required(VERSION 3.22)

include(ExternalProject)

project(OpenOCD)

set(OPENOCD_TESTSUITE_INSTALL_PATH ${CMAKE_BINARY_DIR}/install_testsuite)

get_filename_component(OPENOCD_SOURCES ${CMAKE_SOURCE_DIR}/../.. ABSOLUTE)

set(TESTSUITE_NAME ocd_transferable_testsuite)
add_custom_target(
  transferable_testsuite
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/utils/make_distr.sh
          ${CMAKE_BINARY_DIR}
          ${OPENOCD_TESTSUITE_INSTALL_PATH}
          ${TESTSUITE_NAME}
          ${OPENOCD_INSTALL_PATH}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

include(cmake/DeclareLinuxTests.cmake)
