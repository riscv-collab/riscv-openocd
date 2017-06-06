These scripts are used during the **GNU MCU Eclipse OpenOCD** build procedure. 

For the build script itself, see https://github.com/gnu-mcu-eclipse/build-scripts.

## cross-pkg-config

For MinGW-w64 cross builds it is necessary to use a custom pkg-config, that will search only in the cross toolchain location, and never search the standard system locations, otherwise cross definitions will be messed with native definitions.

## pkg-config-dbg 

When using custom pkg-config files, and you are not sure the configure or cmake understand them correctly, you can temporarily use a debugging version of pkg-config that will dump more info on stderr:

For `cmake`, add the following to the list of command line options:

```
-DPKG_CONFIG_EXECUTABLE="${OPENOCD_GIT_FOLDER}/gnu-mcu-eclipse/scripts/pkg-config-dbg" \ 
```

For `configure`, add the following to the environment before invoking configure:

```
PKG_CONFIG="${OPENOCD_GIT_FOLDER}/gnu-mcu-eclipse/scripts/pkg-config-dbg" \ 
```

## cmake --trace

To diagnose cmake scripts, temporarily add `--trace` when invoking `cmake`.

