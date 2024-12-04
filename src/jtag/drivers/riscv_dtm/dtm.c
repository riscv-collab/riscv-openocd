#include "dtm.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <helper/command.h>
#include <helper/log.h>
#include "jtag/interface.h"
#include "jtag/riscv_socket_dmi.h"

#define MAX_DTM_DRIVERS 10

static dtm_driver_t *registered_drivers[MAX_DTM_DRIVERS];
static int driver_count = 0;

static dtm_driver_t *active_driver = NULL;

int register_dtm_driver(dtm_driver_t *driver) {
    if (driver_count >= MAX_DTM_DRIVERS) {
        fprintf(stderr, "Maximum number of DTM drivers reached.\n");
        return ERROR_FAIL;
    }
    registered_drivers[driver_count++] = driver;
    return ERROR_OK;
}

static int riscv_dtm_initialize(void) {
    struct transport *current_transport = get_current_transport();
    if (!current_transport) {
        LOG_ERROR("No transport selected.");
        return ERROR_FAIL;
    }

    const char *transport_name = current_transport->name;

    if (select_dtm_driver(transport_name) != ERROR_OK) {
        LOG_ERROR("Failedriverd to select DTM driver '%s'", transport_name);
        return ERROR_FAIL;
    }

    dtm_driver_t *selected_dtm_driver = get_active_dtm_driver();
    if (!selected_dtm_driver) {
        LOG_ERROR("No DTM driver selected.");
        return ERROR_FAIL;
    }

    if (selected_dtm_driver->set_transport && selected_dtm_driver->set_transport(current_transport) != ERROR_OK) {
        LOG_ERROR("Failed to set transport for DTM driver '%s'", selected_dtm_driver->name);
        return ERROR_FAIL;
    }
    LOG_INFO("DTM Adapter initialized with transport '%s'", transport_name);

    return ERROR_OK;
}


static int riscv_dtm_init(void) {
    // register all of the possible transports here that this adapter supports based on the current_transport name
    if (register_riscv_socket_transport() != ERROR_OK) {
        return ERROR_FAIL;
    }
    // if (current_transport && strcmp(current_transport->name, "pcie") == 0) {
    //     return register_riscv_pcie_transport();
    // }
    return riscv_dtm_initialize();
}

// Retrieve a DTM driver by name
dtm_driver_t *get_dtm_driver(const char *name) {
    for (int i = 0; i < driver_count; i++) {
        if (strcmp(registered_drivers[i]->name, name) == 0) {
            return registered_drivers[i];
        }
    }
    return NULL;
}

int select_dtm_driver(const char *name) {
    dtm_driver_t *driver = get_dtm_driver(name);
    if (!driver) {
        fprintf(stderr, "DTM driver '%s' not found.\n", name);
        return ERROR_FAIL;
    }

    if (driver->init(driver) != 0) {
        fprintf(stderr, "Failed to initialize DTM driver '%s'.\n", name);
        return ERROR_FAIL;
    }

    // If the previous active_driver is already in place and it has its own deinit function
    // then we need to deinit it and to set the current one as active with the name from parameter 
    if (active_driver && active_driver->deinit) {
        active_driver->deinit(active_driver);
    }

    active_driver = driver;
    return ERROR_OK;
}

dtm_driver_t *get_active_dtm_driver(void) {
    return active_driver;
}

static int riscv_dtm_free(void) {
    active_driver = get_active_dtm_driver();
    active_driver->deinit(active_driver);
    return ERROR_OK;
}

static const char * const riscv_dtm_transports[] = { "jtag", "socket", "pcie", NULL };

struct adapter_driver riscv_dtm_adapter_driver = {
	.name = "riscv_dtm",
	.transports = riscv_dtm_transports,

	.init = riscv_dtm_init,
	.quit = riscv_dtm_free,
};