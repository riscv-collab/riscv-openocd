# The purpose of this script is just to be a driver for typical fpga-related
# tasks. Like generating list of possible targets and preparation the target
# board for testing
SHELL = /usr/bin/env bash

CI_MK_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

PROJECT_NAME := OpenOCD
WORKING_DIR := fpga_info

.PHONY: prepare_board
prepare_board:
	cmake --build ${WORKING_DIR} --target init_${TARGET_BOARD}

.PHONY: fpga_configuration_registry
fpga_configuration_registry:
	cmake -S ${CI_MK_DIR} -B ${WORKING_DIR} -G "Unix Makefiles"
	cmake --build ${WORKING_DIR} --target fpga_configuration_registry
