#ifndef TARGET__RISCV__SCANS_H
#define TARGET__RISCV__SCANS_H

#include "target/target.h"
#include "jtag/jtag.h"

/* A batch of multiple JTAG scans, which are grouped together to avoid the
 * overhead of some JTAG adapters when sending single commands.  This is
 * designed to support block copies, as that's what we actually need to go
 * fast. */
struct riscv_batch {
	struct target *target;

	size_t allocated_scans;
	size_t used_scans;

	size_t idle_count;

	char *data_out;
	char *data_in;
	struct scan_field *fields;
};

/* Allocates (or frees) a new scan set.  "scans" is the maximum number of JTAG
 * scans that can be issued to this object, and idle is the number of JTAG idle
 * cycles between every real scan. */
struct riscv_batch *riscv_batch_alloc(struct target *target, size_t scans, size_t idle);
void riscv_batch_free(struct riscv_batch *batch);

/* Checks to see if this batch is full. */
bool riscv_batch_full(struct riscv_batch *batch);

/* Executes this scan batch. */
void riscv_batch_run(struct riscv_batch *batch);

/* Adds a DMI write to this batch. */
void riscv_batch_add_dmi_write(struct riscv_batch *batch, unsigned address, uint64_t data);

#endif
