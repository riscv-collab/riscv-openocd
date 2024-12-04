// Defines the abstract interface for the Debug Transport Module (DTM)

#ifndef RISCV_TRANSPORT_DTM_H
#define RISCV_TRANSPORT_DTM_H

#include <stdint.h>
#include <stdbool.h>
#include "../../../transport/transport.h"

typedef struct dtm_driver {
    const char *name; // e.g., "jtag", "pcie", "socket"
    int (*init)(struct dtm_driver *driver);
    int (*deinit)(struct dtm_driver *driver);
    int (*read_dmi)(struct dtm_driver *driver, uint32_t *data, uint32_t address);
    int (*write_dmi)(struct dtm_driver *driver, uint32_t address, uint32_t data);
    int (*read_dtmcontrol)(struct dtm_driver *driver, uint32_t* value);
    int (*set_transport)(struct transport *transport);
    void *priv;  // Private data for the driver
} dtm_driver_t;

int register_dtm_driver(dtm_driver_t *driver);

dtm_driver_t *get_dtm_driver(const char *name);

int select_dtm_driver(const char *name);

dtm_driver_t *get_active_dtm_driver(void);

#endif /* RISCV_TRANSPORT_DTM_H */