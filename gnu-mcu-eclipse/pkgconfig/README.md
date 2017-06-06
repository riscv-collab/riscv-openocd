These are the configuration files missing in some of the official packages.

* `hidapi-0.7.0-*.pc` - configuration files for `libhid`;
* `libusb-win32-1.2.6.0.pc` - configuration file for `libusb-w32`.

These files are copied to the internal install folder and will be used by `pkg-config` to configure the compiler and linker settings.
