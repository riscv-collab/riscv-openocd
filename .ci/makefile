# This makefile only has helper targets, it is not meant to handle dependencies --- this is cmake's job
SHELL = /usr/bin/env bash

CI_MK_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

PROJECT_NAME := OpenOCD

TARGET_BOARD ?=
BUILD_TYPE ?= Release

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

.PHONY: config
config:
	./bootstrap
	cmake -S .ci -B $(COMMON_BUILD_DIR) -G "Unix Makefiles"

.PHONY: build
build: config
	cmake --build $(COMMON_BUILD_DIR) --parallel $(PARALLEL)

.PHONY: test
test: build
	cmake --build $(COMMON_BUILD_DIR) --target OpenOCDTest

.PHONY: prepare_board
prepare_board:
	cmake -S .ci/fpga_support -B $(HOST_CONFIGURATION_DIRECTORY) -G "Unix Makefiles"
	cmake --build $(HOST_CONFIGURATION_DIRECTORY) --target board_${TARGET_BOARD}


.PHONY: install
install:


.PHONY: test_install
test_install:


.PHONY: package_name
package_name:


.PHONY: package
package:


.PHONY: clean
clean:
	cmake --build "$(COMMON_BUILD_DIR)" --target clean $(VERBOSE_OPTION)

###################### docker support ######################
TARGET ?=
COMMAND ?=
DOCKER_ADMIN ?= false
DOCKER_CACHE ?= $$HOME/.cache
DOCKER_REGISTRY ?= nexus.dev.syntacore.com:8091
DOCKER_CONTAINER_NAME ?= $(PROJECT_NAME)_container

DOCKER_KEEP_CI_USER_SUDO := true
DOCKER_IMAGE_TAG := sc/docker_cpp:1.0.10
DOCKER_CONTAINER := $(COMMON_BUILD_DIR)/$(DOCKER_CONTAINER_NAME)
DOCKER_CONTAINER_USER != [[ "$(DOCKER_ADMIN)" != "true" ]] && echo "ci_user" || echo "root"
DOCKER_CONTAINER_USER_HOME != [[ "$(DOCKER_CONTAINER_USER)" != "root" ]] && echo "/home/$(DOCKER_CONTAINER_USER)" || echo "/root"
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
		--env KEEP_CI_USER_SUDO="$(DOCKER_KEEP_CI_USER_SUDO)" \
		--env CI_UID="$$(id --user)" --env CI_GID="$$(id --group)" \
		--env "TERM=xterm-256color" \
		--user root \
		--name $(DOCKER_CONTAINER_NAME) \
		--mount type=bind,source="$$(dirname $(CI_MK_DIR))",target=/home/repo \
		--mount type=bind,source="$$(realpath $(COMMON_BUILD_DIR))",target=/home/build \
		--mount type=bind,source="$$(realpath $(DOCKER_CACHE))",target=$(DOCKER_CONTAINER_USER_HOME)/.cache \
		$(DOCKER_REGISTRY)/$(DOCKER_IMAGE_TAG)
	sleep 1
	touch $@

.PHONY: container
container: $(DOCKER_CONTAINER)

.PHONY: in_docker
in_docker: $(DOCKER_CONTAINER)
	docker exec \
		--env TARGET_BOARD="$(TARGET_BOARD)" \
		--env PRESET="$(PRESET)" \
		--env COMMON_BUILD_DIR=/home/build \
		--env NAS_USER="$(NAS_USR)" \
		--env NAS_PASS="$(NAS_PSW)" \
		--env BUILD_TYPE="$(BUILD_TYPE)" \
		\
		--env PARALLEL="$(PARALLEL)" \
		--env VERBOSE="$(VERBOSE)" \
		\
		--env COMMIT="$(COMMIT)" \
		--env CLANG_FORMAT_OUTPUT="$(CLANG_FORMAT_OUTPUT)" \
		--env CLANG_TIDY_OUTPUT="$(CLANG_TIDY_OUTPUT)" \
		\
		--env CI_UID="$$(id --user)" --env CI_GID="$$(id --group)" \
		--user $(DOCKER_CONTAINER_USER) \
		$(DOCKER_CONTAINER_NAME) \
		bash -c '$(DOCKER_COMMAND)'

.PHONY: clean_docker
clean_docker:
	docker container ls --quiet --all --filter name='^/$(DOCKER_CONTAINER_NAME)_' | ifne xargs docker stop
	docker container ls --quiet --all --filter name='^/$(DOCKER_CONTAINER_NAME)_' | ifne xargs docker rm

