RISC-V Binary Tools for Embedded Development
--------

At SiFive we've been distributing binary releases of the embedded development
tools that target our RISC-V platforms.  This repository contains the scripts
we use to build these tools.

You should just be able to type "make" and get all the toolchain releases
supported on the current platform.  Right now this just builds
riscv-gnu-toolchain for both Windows and Ubuntu, but I plan on adding other
targets as we perform the first release.

The end-user output is a set of tarballs in "bin" that should be ready to
upload to a website or deliver to customers.
