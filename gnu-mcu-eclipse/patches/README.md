These are small patches required to correct some problems identified in the official packages.

* `libftdi1-1.2-cmake-FindUSB1.patch` - adds `NO_DEFAULT_PATH` to `FIND_PATH()` and `FIND_LIBRARY()`, to make cmake not search libusb in system default locations;

* `libusb-win32-1.2.6.0-mingw-w64.patch` - makes `libusb-win32` build without the Microsoft DDK; it was copied from the [JTAG Tools](https://gitorious.org/jtag-tools/openocd-mingw-build-scripts) project, and repuires `-p1` when applying.

# Memo

To create a patch:

```bash
$ cd top
$ cp folder/file folder/file.patched
$ vi folder/file.patched
$ diff -u folder/file folder/file.patched >my.patch
```

To apply the patch:

```bash
$ cd top
$ patch -p0 <my.patch
```
