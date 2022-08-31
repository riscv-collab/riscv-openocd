# Introduction

This is a testsuite designed to test `openocd`. It can run openocd tests
on a simulator or baremetal targets (given that a proper board description
is provided). Tests are organized into categories for convenience.

Test are written in tcl (with **expect** extension) and use dejagnu framework
for test harness.

See **Resources** for useful links.

To run the testsuite on a paricular board
(like spike simulator or baremetal target) the following command is used:

```
# Prerequisites :
# 1. make sure that you have riscv64-unknown-elf-gcc in your PATH.
# 2. set `OPENOCD_ROOT` environment variable to openocd installation path.
# 3. Optional. To run tests on a simulator make sure that spike is in your PATH.

# WARNING: consider running tests in a separate directory since most likely
#          you don't want your files to be accidentally overwritten.

DEJAGNU=<PATH_TO_SITE_EXP> \
  runtest --tool ocd --srcdir <TESTSUITE_DIR> \
  --target_board <board_name>

# <PATH_TO_SITE_EXP> - path to site.exp conifiguration file, found at
($SRCDIR/site.exp).
# <TESTSUITE_DIR> - path to the `$SRCDIR/testsuite` directory.
# <board_name> - name of the board. For example "spike_dmp6a0_64".
# runtest - this program is part of dejagnu distribution.
```

**target_boad** argument can be ommitted. In this case default targets
are selected - currently, these correspond to all spike configurations
distributed with the testsuite.

# Dependencies:

* riscv toolchain
* gdb with RISC-V support
* openocd binaries with RISC-V support
* spike simulator
* except package
* dejagnu distribution
* netcat (nc).

# Overall architecture

Dejagnu has 2 distinct concepts:

* the tool - represents the program (and associated environment) to test.
* the board - loosely represents the target (and associated environment) on
which tests are run. It can be something simple like a simulator or a dedicated
stand with an FPGA.

Dejagnu user provides:

- proper description of the board. These descriptions go to `$testsuite/boards`
- provides tool-specific functionality. This goes to `$testsuite/lib/$tool.exp`
- testfiles to run. These go to `$testsuite/${board}.$category`

Tool-specific functionality can query board configurations and adjust the
behavior based on that. Test programs use API provided by tool support library
and can query board-specific information.

When tests are run, the following happens:

1. runtest program loads board configuration and calls `${board}_init` hook
   that can be provided by a board description. The intention is to initialize
   the board: launch simulator or instantiate bitstream on an FPGA.
   See `$SRCDIR/config/sim.exp` for an example.

2. compose a list of test files to run.

3. For each test file do the following:

  * call `${tool}_init`. This comes from `$testsuite/lib/${tool}.exp` file.
  The intention is to prepair tool-specific environement before running the
  test.
  * execute the test file.
  * call `${tool}_finish`. This comes from `$testsuite/lib/${tool}.exp`

4. Once execution of all test files is complete - `${board}_close` is called.

5. If there are more boards to test - go to step 1.

# Implementation peculiarities

## Execution and integration

Dejagnu-based tests can be run in two different ways:

* they can be integrated into a build system of the project (via dedicated
autoconf/automake facilities)
* tests can be run directly via runtests utility.

We support only the latter.

## Remote targets

Dejagnu provides rich facilities for remote test execution: target board can be
on a remote Windows system, etc. Currently, we don't use any of that
(to simplify things).

Testsuite assumes that all the required tools are available locally. In order
to communicate with openocd test driver can either:

* connect to openocd-provided gdb-server (if gdb is the driver for the test)
* connect to openocd-provided TCL-RPC server via netcat (to issue openocd
commands directly).

## Communication with openocd

The tool support library provides facilities to parse responses from TCL-RPC
server. In future we may want to connect to telnet server, instead of the tcl
one. TCL-RPC server was choosen to simplify implementation - since it provides
a clean way to identify the end of the message in a response stream.
These facilities reside in `$testsuite/lib/response_parser.exp`.

Parsers are by no means complete but are good enough to get things up and
running. It is expected that these will be enhanced once the test suite is more
mature.

# Resources

Tcl:

  * Tcl Language description (~100p):
    https://www.ee.columbia.edu/~shane/projects/sensornet/part1.pdf

Except:

  * Very good introduction/description (~600p): "Exploring Expect" by Don Libes

Dejagnu:

  To be fair, dejagnu documentation is not very good. I've found that it's
  easier just to read the source code (it's not that big to understand what is
  going on). Still, here are some resources I've found usefull:

  * basic documentation:
    https://www.gnu.org/software/dejagnu/dejagnu.pdf

  * relatively gentle introduction:
    https://www.embecosm.com/appnotes/ean8/ean8-howto-dejagnu-1.0.html#sec_config

  * Some rants regarding poor design of dejagnu:
    https://www.airs.com/blog/archives/499

