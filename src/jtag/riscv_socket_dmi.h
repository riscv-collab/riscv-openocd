#ifndef SOCKET_DMI_H
#define SOCKET_DMI_H

#include "drivers/riscv_dtm/dtm.h"
#include "helper/command.h"
#include <stdint.h>

int socket_dmi_init(dtm_driver_t *driver);
int socket_dmi_deinit(dtm_driver_t *driver);
int socket_dmi_read_dmi(dtm_driver_t *driver, uint32_t *data, uint32_t address);
int socket_dmi_write_dmi(dtm_driver_t *driver, uint32_t address, uint32_t data);
int socket_dtmcontrol_read_dmi(struct dtm_driver *driver, uint32_t *value);
int register_socket_dtm_driver(void);


int register_riscv_socket_transport(void);

int socket_dmi_read(struct target *target, uint32_t *data, uint32_t address);
int socket_dmi_write(struct target *target, uint32_t address, uint32_t data);
bool transport_is_socket(void);
#endif // SOCKET_DMI_H