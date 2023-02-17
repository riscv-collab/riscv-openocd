# This makefile only has helper targets, it is not meant to handle dependencies --- this is cmake's job
SHELL = /usr/bin/env bash

CI_MK_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

PROJECT_NAME := OpenOCD

TARGET_BOARD ?=
BUILD_TYPE ?= Release
BUILD_ID ?=
STAND_ID ?=
PARALLEL ?= ON

DOWNLOAD_DEPENDENCIES_FROM_ARTIFACTORY ?= false
SYNTACORE_NAS_SERVER ?= nas.dev.syntacore.com

export DOWNLOAD_DEPENDENCIES_FROM_ARTIFACTORY
export SYNTACORE_NAS_SERVER

ifndef COMMON_BUILD_DIR
$(error COMMON_BUILD_DIR is not set)
endif
HOST_CONFIGURATION_DIRECTORY = $(COMMON_BUILD_DIR)/host_tools

BUILD_DIR = $(COMMON_BUILD_DIR)/$(BUILD_TYPE)
BUILD_TARGET ?= test
INSTALL_DIR = $(BUILD_DIR)/install

CORES != nproc --all
PARALLEL != [[ "$(PARALLEL)" == "ON" ]] && echo "$(CORES)" || echo "$(PARALLEL)"
VERBOSE_OPTION != [[ "$(VERBOSE)" == "ON" ]] && echo "--verbose"

.PHONY: all
all: \
	config \
	build \
	test \
	riscv-tests-debug \

VENV := source $(COMMON_BUILD_DIR)/.venv/bin/activate

# NOTE: transitive list of requirements can be obtained using
# `pip3 freeze -r requirements.txt | sponge requirements.txt`
$(COMMON_BUILD_DIR)/.venv: .ci/requirements.txt
	python3 -m venv $(COMMON_BUILD_DIR)/.venv
	$(VENV) && pip3 install -r .ci/requirements.txt
	$(VENV) && pip3 freeze

configure: $(COMMON_BUILD_DIR)/.venv
	$(VENV) && ./bootstrap

.PHONY: config_linux
config_linux: configure
	$(VENV) && cmake -S .ci -B $(COMMON_BUILD_DIR)/linux   -G "Unix Makefiles"

.PHONY: config_windows
config_windows: configure
	$(VENV) && cmake -S .ci -B $(COMMON_BUILD_DIR)/windows -G "Unix Makefiles" \
		--toolchain cmake/ToolchainMingw.cmake

.PHONY: build_linux
build_linux: config_linux
	$(VENV) && cmake --build $(COMMON_BUILD_DIR)/linux --target openocd --parallel $(PARALLEL)

.PHONY: build_windows
build_windows: config_windows
	$(VENV) && cmake --build $(COMMON_BUILD_DIR)/windows --target openocd --parallel $(PARALLEL)

.PHONY: build
build: build_linux build_windows


.PHONY: test
test: build
	$(VENV) && cmake --build $(COMMON_BUILD_DIR)/linux --target OpenOCDTest --parallel $(PARALLEL)

.PHONY: riscv-tests-debug
riscv-tests-debug: build
	$(VENV) && cmake --build $(COMMON_BUILD_DIR)/linux --target RISCVTestsDebug --parallel $(PARALLEL)

.PHONY: install
install:

.PHONY: test_install
test_install:

.PHONY: package_name
package_name:

.PHONY: package
package: build
	$(VENV) && cmake --build "$(COMMON_BUILD_DIR)/linux" --target OpenOCDPackage
	$(VENV) && cmake --build "$(COMMON_BUILD_DIR)/windows" --target OpenOCDPackage

.PHONY: upload_test_report
upload_test_report:
	$(VENV) && cmake --build "$(COMMON_BUILD_DIR)/linux" --target OpenOCDTestReport

.PHONY: record_test_success
upload_test_report:
	$(VENV) && cmake --build "$(COMMON_BUILD_DIR)/linux" --target OpenOCDTestSuccess

.PHONY: clean
clean:
	$(VENV) && cmake --build "$(COMMON_BUILD_DIR)" --target clean $(VERBOSE_OPTION)

.PHONY: prepare_board
prepare_board:
	cmake -S .ci/fpga_support -B $(HOST_CONFIGURATION_DIRECTORY) -G "Unix Makefiles"
	cmake --build $(HOST_CONFIGURATION_DIRECTORY) --target board_${TARGET_BOARD}


###################### docker support ######################
TARGET ?=
COMMAND ?=
DOCKER_CACHE ?= $$HOME/.cache
DOCKER_REGISTRY ?= nexus.dev.syntacore.com:8091
DOCKER_CONTAINER_NAME ?= $(PROJECT_NAME)_container

DOCKER_IMAGE_TAG := sc/cpp_ubuntu:18.1.0
DOCKER_CONTAINER := $(COMMON_BUILD_DIR)/$(DOCKER_CONTAINER_NAME)
DOCKER_COMMAND != [[ ! -z '$(COMMAND)' ]] && echo '$$(COMMAND)' || echo 'make $$(TARGET) -f .ci/makefile'


IF_DOCKERD_UP := command -v docker &> /dev/null && docker image ls &> /dev/null

DOCKER_CONTAINER_ID != $(IF_DOCKERD_UP) && docker container ls --quiet --all --filter name=^/$(DOCKER_CONTAINER_NAME)$
DOCKER_CONTAINER_STATE != $(IF_DOCKERD_UP) && docker container ls --format {{.State}} --all --filter name=^/$(DOCKER_CONTAINER_NAME)$
DOCKER_CONTAINER_RUN_STATUS != [[ "$(DOCKER_CONTAINER_STATE)" != "running" ]] && echo "$(DOCKER_CONTAINER)_not_running"

.PHONY: $(DOCKER_CONTAINER)_not_running
$(DOCKER_CONTAINER): $(DOCKER_CONTAINER_RUN_STATUS)
ifneq ($(DOCKER_CONTAINER_ID),)
	docker container rename $(DOCKER_CONTAINER_NAME) $(DOCKER_CONTAINER_NAME)_$(DOCKER_CONTAINER_ID)
endif
	mkdir --parents "$(DOCKER_CACHE)"
	mkdir --parents "$(COMMON_BUILD_DIR)"
	find "$(COMMON_BUILD_DIR)" -type d -empty
	docker run --interactive --tty --detach \
		--privileged \
		--env "TERM=xterm-256color" \
		--user $$(id -u):$$(id -g) \
		--name $(DOCKER_CONTAINER_NAME) \
		--mount type=bind,source="$$(dirname $(CI_MK_DIR))",target=/home/repo \
		--mount type=bind,source="$$(realpath $(COMMON_BUILD_DIR))",target=/home/build \
		--workdir "/home/repo" \
		$(DOCKER_REGISTRY)/$(DOCKER_IMAGE_TAG)
	docker exec --user root $(DOCKER_CONTAINER_NAME) \
		bash -c "mkdir --parents $$HOME"
	docker exec --user root $(DOCKER_CONTAINER_NAME) \
		bash -c "chown $$(id -u):$$(id -g) $$HOME"
	docker exec --user root $(DOCKER_CONTAINER_NAME) \
		bash -c "groupadd --gid $$(id -g) $$(id -u -n)"
	docker exec --user root $(DOCKER_CONTAINER_NAME) \
		bash -c "useradd --uid $$(id -u) --gid $$(id -g) $$(id -u -n)"
	touch $@

.PHONY: container
container: $(DOCKER_CONTAINER)

.PHONY: in_docker
in_docker: $(DOCKER_CONTAINER)
	docker exec \
		--env TARGET_BOARD="$(TARGET_BOARD)" \
		--env BUILD_ID="$(BUILD_ID)" \
		--env STAND_ID="$(STAND_ID)" \
		\
		--env PRESET="$(PRESET)" \
		--env COMMON_BUILD_DIR="/home/build" \
		--env SYNTACORE_NAS_SERVER="$(SYNTACORE_NAS_SERVER)" \
		--env DOWNLOAD_DEPENDENCIES_FROM_ARTIFACTORY="$(DOWNLOAD_DEPENDENCIES_FROM_ARTIFACTORY)" \
		--env ARTIFACTORY_API_KEY="$(ARTIFACTORY_API_KEY)" \
		--env BUILD_TYPE="$(BUILD_TYPE)" \
		\
		--env PARALLEL="$(PARALLEL)" \
		--env VERBOSE="$(VERBOSE)" \
		\
		--env COMMIT="$(COMMIT)" \
		\
		--user "$$(id -u -n)" \
		$(DOCKER_CONTAINER_NAME) \
		bash -c '$(DOCKER_COMMAND)'

.PHONY: clean_docker
clean_docker:
	docker container ls --quiet --all --filter name='^/$(DOCKER_CONTAINER_NAME)_' | ifne xargs docker stop
	docker container ls --quiet --all --filter name='^/$(DOCKER_CONTAINER_NAME)_' | ifne xargs docker rm

