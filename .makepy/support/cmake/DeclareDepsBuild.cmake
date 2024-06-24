function(declare_build_dependencies target)
  set(libjaylink_src_code libjaylink)

  # NOTE: pkg_config_path is used because modern distributions do not have
  # static version of libudev
  set(pkg_config_path ${DEPENDENCIES_INSTALL_PATH}/lib/pkgconfig)

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
      env PKG_CONFIG_PATH=${pkg_config_path}:$ENV{PKG_CONFIG_PATH}
          CC=${CMAKE_C_COMPILER}
          CFLAGS=$<$<BOOL:${CMAKE_SYSROOT}>:--sysroot=${CMAKE_SYSROOT}>
      ./autogen.sh
    CONFIGURE_COMMAND
      env PKG_CONFIG_PATH=${pkg_config_path}:$ENV{PKG_CONFIG_PATH}
          CC=${CMAKE_C_COMPILER}
          CFLAGS=$<$<BOOL:${CMAKE_SYSROOT}>:--sysroot=${CMAKE_SYSROOT}>
      ${CMAKE_BINARY_DIR}/libjaylink_Sources/configure
        --host=${CONFIGURE_HOST}
        --prefix=${DEPENDENCIES_INSTALL_PATH}
        --disable-shared
  )
  # cmake-format: on

  add_custom_target(${target} DEPENDS libjaylink)
endfunction()
