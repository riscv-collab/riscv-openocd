# Introduction

This is a test suite designed to test `openocd`. It can run openocd tests on a 
simulator or on baremetal targets (given that a proper board description is 
provided). The tests are organized by category.

Tests are written in tcl (with the **expect** extension) and use the 
DejaGnu framework for the test harness.

See **Resources** for useful links.

## Prerequisites

1. DejaGnu 1.6.3 must be available in your path.

   **NOTE**: Currently, Linux distributives provide an older version of the 
   tool. Install DejaGnu 1.6.3 from the respective sources.

2. Verify that you have the `riscv64-unknown-elf-gcc` toolchain.
3. Verify that your GNU GDB debugger is able to communicate with RISC-V 
targets.
4. To run tests on a simulator, verify that you have Spike available.
5. Verify a `LOCAL_INIT` file is created. Its format is described below.

## Running Test

**WARNING**: Consider running tests in a separate directory to avoid 
accidentally overwriting your files.

To run the test suite on a specific board (for example, a Spike simulator or 
a baremetal target), use the following command:

```
DEJAGNU=<PATH_TO_SITE_EXP> \
  runtest --tool ocd \
  --local_init <PATH_TO_LOCAL_INIT> \
  --srcdir <TESTSUITE_DIR> \
  --target_board <board_name>
```
where: 
- `<PATH_TO_SITE_EXP>` — Path to the `site.exp` configuration file 
(see `$SRCDIR/site.exp`).
-  `runtest` — A utility of the DejaGnu distribution that runs the tests.
- `<PATH_TO_LOCAL_INIT>` — Path to the file that contains information about
 where to find GCC, OpenOCD, and, optionally, the Spike simulator.
- `<TESTSUITE_DIR>` — Path to the `$SRCDIR/testsuite` directory. This parameter
 is mandatory. See the list of supported boards in `${TESTSUITE_DIR}/boards`.
- `<board_name>` — Name of the board for which you want to run the test. 
For example, `spike_dmp6a0_64`.

If necessary, you can specify the default list of boards to use in your 
`site.exp` file. To do this, set the `target_list` variable to the values you 
want, for example:

```
set target_list { board1 board2 }
```

This is not done in the default `site.exp` file to simplify maintenance.

## `LOCAL_INIT`

The `LOCAL_INIT` file sets TCL variables to provide paths to the required tools.
It must look as follows:

```
# openocd installation prefix
set OPENOCD_ROOT /home/user/utils/install_openocd
# path to spike binary
set SPIKE_SIM    /home/user/utils/spike/bin/spike
# path to gdb binary
set GDB_BIN      /home/user/utils/riscv-gcc/bin/riscv64-unknown-elf-gdb
# path to gcc compiler for the target (typically riscv64-unknown-elf-gcc)
set CC_FOR_TARGET /home/user/utils/riscv-gcc/bin/riscv64-unknown-elf-gcc
```

### Dependencies

* RISC-V toolchain
* GDB with RISC-V support
* OpenOCD binaries with RISC-V support
* Spike simulator
* Except package (5.45.4)
* DejaGnu distribution (1.6.3)
* netcat (nc).

## Test Suite Architecture

Dejagnu has 2 distinct concepts:

* **Tool** — Represents the program and its associated environment to test.
* **Board** — Loosely represents the target and its associated environment on 
which the tests are run. It can be something simple (for example, a simulator)
or a dedicated stand with an FPGA.

A DejaGnu user provides:

- Proper description of the board. These descriptions go to `$testsuite/boards`.
- Tool-specific functionality. It goes to `$testsuite/lib/$tool.exp`.
- Testfiles to run. They go to `$testsuite/${board}.$category`.

The tool-specific functionality can query board configurations and adjust the
behavior based on that. Test programs use the API provided by the tool support
library and can query board-specific information.

When tests are run:

1. The `runtest` utility loads the board configuration and calls the 
`${board}_init` hook that can be provided by a board description. The intention
is to initialize the board: launch a simulator or instantiate the bitstream on 
an FPGA.

See `$SRCDIR/config/sim.exp` for an example.

2. Compose a list of test files to run.

3. For each test file, do the following:
   1. Call `${tool}_init`. This comes from the `$testsuite/lib/${tool}.exp` file.
The intention is to prepare a tool-specific environment before running the test.
   2. Execute the test file.
   3. Call `${tool}_finish`. This comes from `$testsuite/lib/${tool}.exp`

4. Once all tests are executed, `${board}_close` is called.

5. If there are more boards to test, go to step 1.

# Implementation Peculiarities

## Execution and Integration

DejaGnu-based tests can be run in two different ways:

* They can be integrated into a build system of the project (via the dedicated 
autoconf/automake facilities).
* They can be run directly via the `runtests` utility.

The current functionality only supports the latter.

## Remote Targets

DejaGnu provides rich facilities for a remote test execution: the target board
can be on a remote Windows system, etc. Currently, to simplify the implementation,
we don't use any of that.

Test suite assumes that all the required tools are available locally. In order
to communicate with OpenOCD, a test driver can either:

* Connect to openocd-provided gdb-server (if GDB is the driver for the test).
* Connect to openocd-provided TCL-RPC server via netcat (to issue low-level
OpenOCD commands directly).

## Communication with OpenOCD

The tool support library provides facilities to parse responses from the TCL-RPC
server. These facilities are located in `$testsuite/lib/response_parser.exp`.

**NOTE**: It is possible that we will support connecting to the telnet server 
instead of the TCL one in future. The TCL-RPC server was selected to simplify
implementation, since it provides a clean way to identify the end of the message
in a response stream.

Currently, parsers are not complete, but are good enough to get things up and
running. It is expected that they will be enhanced once the test suite is more
mature.

## Test Suite Organization

The current implementation provides board definitions to run the test suite on a
Spike simulator. Additionally, the test suite provides utility functions that 
allow to easily add new boards, including support for real debug adapters 
connected to a device.

To extend the test suite with additional boards that require a dedicated lab 
environment and are not available to the general public, create a separate 
directory that contains new board definitions and a `site.exp` file. 
`site.exp` **must** source the original `site.exp` and set `boards_dir` to 
verify that the original `boards` directory that contains definitions for 
simulator boards is included. This is necessary so that new boards could reuse
utility functions distributed with the test suite. For example, see `site.exp`
in `${OPENOCD_SRC}/testing/dejagnu`.

## Private Stand Database

If a board requires a real debug adapter, you need to provide a "private stand 
database". Each record contains the stand name, the debug adapter config, the 
debug adapter serial, and the desired adapter speed. See an example in 
`${OPENOCD_SRC}/testing/dejagnu/boards/embargo/stand_db/example.cfg`.

Board definitions can reference these configuration records the following way:

```
set_board_info <env>,stand <stand_name>
```
See `testing/dejagnu/boards/syntacore` for an example.

# Resources

## TCL
TCL Language description (~100p):
  https://www.ee.columbia.edu/~shane/projects/sensornet/part1.pdf

## Expect
Introduction/description (~600p): "Exploring Expect" by Don Libes.

## DejaGnu

DejaGnu documentation — https://www.gnu.org/software/dejagnu/dejagnu.pdf — does
not provide all the required descriptions. You might find it easier to read the 
source code as it is not too big and is quite straightforward. 

There are some other useful resources:

* Introduction: 
https://www.embecosm.com/appnotes/ean8/ean8-howto-dejagnu-1.0.html#sec_config

* DejaGnu blogs:
  https://www.airs.com/blog/archives/499
