# Syntacore build, test, install and packaging orchestration scripts

## Setup system

Before using these scripts you need to install `python >= 3.10` and `docker >= 23.0.0`
For `ubuntu 22.04`:

```bash
sudo apt-get install python3-venv
sudo apt-get install python3-pip
```

`docker` is [described below](#docker-prerequisite)


## Using build orchestration scripts

You need to install Conan config once.
If conan config in `makepy` is updated you will need to rerun this command.
```bash
./make.py conan-config
```

For Conan on Linux systems use default profile.
If cross-compiling for Windows use `makepy_sc_mingw` profile.
```bash
# Linux -> Linux
PROFILE=default # No need to actually specify it in CLI, it is default well... by default

# Linux -> Windows
PROFILE=makepy_sc_mingw
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
./make.py --image $IMAGE conan source .
./make.py --image $IMAGE conan install --settings:host build_type=$BUILD_TYPE --profile:host $PROFILE .
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
./make.py conan source .
./make.py conan install --settings:host build_type=$BUILD_TYPE --profile:host $PROFILE .
./make.py config --build-path $BUILD_PATH
./make.py build --build-path $BUILD_PATH
./make.py build --build-path $BUILD_PATH --target OpenOCDTest
./make.py build --build-path $BUILD_PATH --target RISCVTestsDebug
```

### Option 3: Manually config your system
**Requirements:** ...

```bash
./make.py conan source .
./make.py conan install --settings:host build_type=$BUILD_TYPE --profile:host $PROFILE .
./make.py config --build-path $BUILD_PATH
./make.py build --build-path $BUILD_PATH
./make.py build --build-path $BUILD_PATH --target OpenOCDTest
./make.py build --build-path $BUILD_PATH --target RISCVTestsDebug
```

## Docker prerequisite

You need to properly setup docker engine to use scripts with container option.

1. Install docker. Refer to official installation guide. [Click here for Ubuntu](https://docs.docker.com/engine/install/ubuntu/).
2. Add yourself to docker group:

```bash
sudo usermod -aG docker "$USER"
```

3. Configure the engine.
Add this to `/etc/docker/daemon.json`:

```json
{
  "insecure-registries": ["nexus.dev.syntacore.com:8091"]
}
```

4. Reboot your machine.
