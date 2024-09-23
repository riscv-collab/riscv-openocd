# Syntacore build, test, install and packaging orchestration scripts

## Preparation

First, setup your system according to the [makepy readme](http://gitlab.dev.syntacore.com/tools/makepy#makepy).

For Conan on Linux systems use default profile.
If cross-compiling for Windows use `makepy_sc_mingw` profile.

```bash
# Linux -> Linux
PROFILE=default # No need to actually specify it in CLI, it is default well... by default

# Linux -> Windows
PROFILE=mp_mingw
```

Also choose build type and image if running in container.

```bash
BUILD_TYPE=Release
BUILD_PATH=build/$BUILD_TYPE
IMAGE=cpp_ubuntu_18
```

Three different ways you can use build scripts:

### Option 1: Send commands to docker container

**Requirements:** `python`, `docker`

```bash
./make.py --image $IMAGE container run
./make.py --image $IMAGE conan source
./make.py --image $IMAGE conan install --settings:host "&:build_type=$BUILD_TYPE" --profile:host $PROFILE
./make.py --image $IMAGE config --build-path $BUILD_PATH
./make.py --image $IMAGE build --build-path $BUILD_PATH
./make.py --image $IMAGE build --build-path $BUILD_PATH --target OpenOCDTest
./make.py --image $IMAGE build --build-path $BUILD_PATH --target RISCVTestsDebug
```

### Option 2: Use docker container interactively

**Requirements:** `python`, `docker`

```bash
./make.py --image $IMAGE container run
./make.py --image $IMAGE sh bash
./make.py conan source
./make.py conan install --settings:host "&:build_type=$BUILD_TYPE" --profile:host $PROFILE
./make.py config --build-path $BUILD_PATH
./make.py build --build-path $BUILD_PATH
./make.py build --build-path $BUILD_PATH --target OpenOCDTest
./make.py build --build-path $BUILD_PATH --target RISCVTestsDebug
```

### Option 3: Manually config your system

**Requirements:** ...

```bash
./make.py conan source
./make.py conan install --settings:host '&:build_type=$BUILD_TYPE' --profile:host $PROFILE
./make.py config --build-path $BUILD_PATH
./make.py build --build-path $BUILD_PATH
./make.py build --build-path $BUILD_PATH --target OpenOCDTest
./make.py build --build-path $BUILD_PATH --target RISCVTestsDebug
```
