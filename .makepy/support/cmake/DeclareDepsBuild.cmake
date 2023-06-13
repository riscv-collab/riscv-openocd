function(declare_build_dependencies target)
  set(libusb_src_code libusb)
  set(libftdi_src_code libftdi)
  set(libhidapi_src_code hidapi)
  set(libjaylink_src_code libjaylink)

  # NOTE: pkg_config_path is used because modern distributions do not have
  # static version of libudev
  set(pkg_config_path ${DEPENDENCIES_INSTALL_PATH}/lib/pkgconfig)

  # LIBUSB
  # cmake-format: off
  ExternalProject_Add(
    libusb
    PREFIX libusb_Build
    SOURCE_DIR libusb_Sources
    URL file://${DEPENDENCIES_LOCATION}/${libusb_src_code}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    CONFIGURE_COMMAND
      env
        PKG_CONFIG_PATH=${pkg_config_path}
        CC=${CMAKE_C_COMPILER}
      ${CMAKE_BINARY_DIR}/libusb_Sources/configure
        --host=${CONFIGURE_HOST}
        --prefix=${DEPENDENCIES_INSTALL_PATH}
        --disable-shared
        --with-pic &&
      touch
        ${CMAKE_BINARY_DIR}/libusb_Sources/Makefile.in
        ${CMAKE_BINARY_DIR}/libusb_Sources/aclocal.m4
  )
  # cmake-format: on

  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(libhidadpi_build_dir "windows")
    set(libhidapi_make_file "Makefile.mingw")
    set(hidapi_pkg_template "hidapi_windows.pc.in")
  else()
    set(libhidadpi_build_dir "linux")
    set(libhidapi_make_file "Makefile")
    set(hidapi_pkg_template "hidapi_linux.pc.in")
  endif()

  set(libhidapi_name libhidapi.a)
  set(libhidapi_pc_tmp hidapi_tmp.pc)
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/dependencies_support/${hidapi_pkg_template}
    ${CMAKE_CURRENT_BINARY_DIR}/${libhidapi_pc_tmp} @ONLY)
  # cmake-format: off
  ExternalProject_Add(
    libhidapi
    PREFIX libhidapi_Build
    SOURCE_DIR libhidapi_Sources
    URL file://${DEPENDENCIES_LOCATION}/${libhidapi_src_code}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    # By default the Makefile is configured for hid-libusb.
    # This sed command reconfigures makefile to hidraw backend
    # (as per documentation).
    PATCH_COMMAND
      cp linux/Makefile-manual linux/Makefile &&
      sed -i s/\ ..\\/hidtest\\/test.o// linux/Makefile
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE TRUE
    BUILD_COMMAND
      cd ${libhidadpi_build_dir} &&
      env PKG_CONFIG_PATH=${pkg_config_path}
      make CC=${CMAKE_C_COMPILER} -f ${libhidapi_make_file} hid.o &&
      ${CMAKE_AR} rc ${libhidapi_name} hid.o &&
      ${CMAKE_RANLIB} ${libhidapi_name} &&
      cp ${libhidapi_name} ../
    INSTALL_COMMAND
      mkdir -p ${DEPENDENCIES_INSTALL_PATH}/lib/pkgconfig &&
      mkdir -p ${DEPENDENCIES_INSTALL_PATH}/include/hidapi &&
      cp ${libhidapi_name} ${DEPENDENCIES_INSTALL_PATH}/lib &&
      cp hidapi/hidapi.h ${DEPENDENCIES_INSTALL_PATH}/include/hidapi &&
      cp ${CMAKE_CURRENT_BINARY_DIR}/${libhidapi_pc_tmp}
        ${DEPENDENCIES_INSTALL_PATH}/lib/pkgconfig/hidapi.pc
    DEPENDS libusb
  )
  # cmake-format: on

  # LIBFTDI
  # cmake-format: off
  # cmake-lint: disable=C0301
  ExternalProject_Add(
    libftdi
    CMAKE_ARGS -DCMAKE_PREFIX_PATH=${DEPENDENCIES_INSTALL_PATH}
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
    URL file://${DEPENDENCIES_LOCATION}/${libftdi_src_code}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    INSTALL_DIR ${DEPENDENCIES_INSTALL_PATH}
    # NOTE: -delete commands are needed to enforce static linking
    INSTALL_COMMAND
      make install &&
      find ${DEPENDENCIES_INSTALL_PATH} -name libftdi*.so* -delete &&
      find ${DEPENDENCIES_INSTALL_PATH} -name libftdi*.dll* -delete &&
      find ${DEPENDENCIES_INSTALL_PATH} -name libftdipp* -delete &&
      :
    DEPENDS libusb
  )
  # cmake-format: on

  # LIBJAYLINK
  # cmake-format: off
  ExternalProject_Add(
    libjaylink
    PREFIX libjaylink_Build
    SOURCE_DIR libjaylink_Sources
    URL file://${DEPENDENCIES_LOCATION}/${libjaylink_src_code}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    # NOTE: we use PATCH_COMMAND as a hack to run autogen.sh. The reason we need
    # it is that this stupid autogen.sh requires us to be in the source code
    # directory. We can't put it in CONFIGURE_COMMAND since we don't know how
    # to return back to the original directory (without even more nastier
    # hacks)
    PATCH_COMMAND
      cd ${CMAKE_BINARY_DIR}/libjaylink_Sources &&
      env PKG_CONFIG_PATH=${pkg_config_path}
          CC=${CMAKE_C_COMPILER}
      ./autogen.sh
    CONFIGURE_COMMAND
      env PKG_CONFIG_PATH=${pkg_config_path}
          CC=${CMAKE_C_COMPILER}
      ${CMAKE_BINARY_DIR}/libjaylink_Sources/configure
        --host=${CONFIGURE_HOST}
        --prefix=${DEPENDENCIES_INSTALL_PATH}
        --disable-shared
    DEPENDS libusb
  )
  # cmake-format: on

  add_custom_target(${target} DEPENDS libusb libhidapi libftdi libjaylink)
endfunction()
