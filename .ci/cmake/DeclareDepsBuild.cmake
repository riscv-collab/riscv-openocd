function(declare_build_dependencies target)

  set(LIBUSB_VERSION    1.0.26)
  set(LIBFTDI_VERSION   1.5)
  set(LIBHIDAPI_VERSION 0.12.0)

  set(LIBUSB_SRC_CODE     libusb-${LIBUSB_VERSION}.tar.bz2)
  set(LIBFTDI_SRC_CODE    libftdi1-${LIBFTDI_VERSION}.tar.bz2)
  set(LIBHIDAPI_SRC_CODE  hidapi-${LIBHIDAPI_VERSION}.tar.gz)

  # NOTE: PKG_CONFIG_PATH is used because modern distributions do not
  # have static version of libudev
  set(PKG_CONFIG_PATH ${DEPENDENCIES_INSTALL_PATH}/lib/pkgconfig)

  # LIBUSB
  ExternalProject_Add(libusb
    PREFIX libusb_Build
    SOURCE_DIR libusb_Sources
    URL file://${DEPENDENCIES_LOCATION}/${LIBUSB_SRC_CODE}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    CONFIGURE_COMMAND
      env PKG_CONFIG_PATH=${PKG_CONFIG_PATH} CC=${CMAKE_C_COMPILER}
        ${CMAKE_BINARY_DIR}/libusb_Sources/configure
        --host=${CONFIGURE_HOST}
        --prefix=${DEPENDENCIES_INSTALL_PATH}
        --disable-shared
        --with-pic
  )

  if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LIBHIDAPI_BUILD_DIR "windows")
    set(LIBHIDAPI_MAKE_FILE "Makefile.mingw")
    set(HIDAPI_PKG_TEMPLATE "hidapi_windows.pc.in")
  else()
    set(LIBHIDAPI_BUILD_DIR "linux")
    set(LIBHIDAPI_MAKE_FILE "Makefile")
    set(HIDAPI_PKG_TEMPLATE "hidapi_linux.pc.in")
  endif()

  set(LIBHIDAPI_NAME libhidapi.a)
  set(LIBHIDAPI_PC_TMP hidapi_tmp.pc)
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/dependencies_support/${HIDAPI_PKG_TEMPLATE}
    ${CMAKE_CURRENT_BINARY_DIR}/${LIBHIDAPI_PC_TMP} @ONLY)
  ExternalProject_Add(libhidapi
    PREFIX libhidapi_Build
    SOURCE_DIR libhidapi_Sources
    URL file://${DEPENDENCIES_LOCATION}/${LIBHIDAPI_SRC_CODE}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    # By default the Makefile is configured for hid-libusb. This sed command
    # reconfigures makefile to hidraw backend (as per documentation)
    PATCH_COMMAND cp linux/Makefile-manual linux/Makefile
    PATCH_COMMAND && sed -i s/\ ..\\/hidtest\\/test.o// linux/Makefile
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE TRUE
    BUILD_COMMAND
         cd ${LIBHIDAPI_BUILD_DIR}
      && env PKG_CONFIG_PATH=${PKG_CONFIG_PATH}
         make CC=${CMAKE_C_COMPILER} -f ${LIBHIDAPI_MAKE_FILE} hid.o
      && ${CMAKE_AR} rc ${LIBHIDAPI_NAME} hid.o
      && ${CMAKE_RANLIB} ${LIBHIDAPI_NAME}
      && cp ${LIBHIDAPI_NAME} ../
    INSTALL_COMMAND
         mkdir -p ${DEPENDENCIES_INSTALL_PATH}/lib/pkgconfig
      && mkdir -p ${DEPENDENCIES_INSTALL_PATH}/include/hidapi
      && cp ${LIBHIDAPI_NAME} ${DEPENDENCIES_INSTALL_PATH}/lib
      && cp hidapi/hidapi.h ${DEPENDENCIES_INSTALL_PATH}/include/hidapi
      && cp ${CMAKE_CURRENT_BINARY_DIR}/${LIBHIDAPI_PC_TMP} ${DEPENDENCIES_INSTALL_PATH}/lib/pkgconfig/hidapi.pc
  )

  # LIBFTDI
  ExternalProject_Add(libftdi
    CMAKE_ARGS
      -DCMAKE_PREFIX_PATH=${DEPENDENCIES_INSTALL_PATH}
      -DCMAKE_INSTALL_PREFIX=${DEPENDENCIES_INSTALL_PATH}
      -DFTDI_EEPROM=OFF
      -DFTDIPP=OFF
      -DBUILD_TESTS=OFF
      -DDOCUMENTATION=OFF
      -DEXAMPLES=OFF
      -DPYTHON_BINDINGS=OFF
      -DLINK_PYTHON_LIBRARY=OFF
      $<$<BOOL:${CMAKE_CROSSCOMPILING}>:-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}>
    PREFIX libftdi_Build
    SOURCE_DIR libftdi_Sources
    URL file://${DEPENDENCIES_LOCATION}/${LIBFTDI_SRC_CODE}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    INSTALL_DIR ${DEPENDENCIES_INSTALL_PATH}
    # NOTE: -delete commands are needed to enforce static linking
    INSTALL_COMMAND
         make install
      && find ${DEPENDENCIES_INSTALL_PATH} -name libftdi*.so* -delete
      && find ${DEPENDENCIES_INSTALL_PATH} -name libftdi*.dll* -delete
      && find ${DEPENDENCIES_INSTALL_PATH} -name libftdipp* -delete
    DEPENDS libusb
  )

  add_custom_target(${target}
    DEPENDS libusb libhidapi libftdi
  )
endfunction()

