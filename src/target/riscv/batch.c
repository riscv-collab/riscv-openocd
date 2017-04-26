#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "batch.h"
#include "debug_defines.h"
#include "riscv.h"

#define get_field(reg, mask) (((reg) & (mask)) / ((mask) & ~((mask) << 1)))
#define set_field(reg, mask, val) (((reg) & ~(mask)) | (((val) * ((mask) & ~((mask) << 1))) & (mask)))

static void dump_field(const struct scan_field *field);

struct riscv_batch *riscv_batch_alloc(struct target *target, size_t scans, size_t idle)
{
	scans += 4;
	struct riscv_batch *out = malloc(sizeof(*out));
	memset(out, 0, sizeof(*out));
	out->target = target;
	out->allocated_scans = scans;
	out->used_scans = 0;
	out->idle_count = idle;
	out->data_out = malloc(sizeof(*out->data_out) * (scans) * sizeof(uint64_t));
	out->data_in  = malloc(sizeof(*out->data_in)  * (scans) * sizeof(uint64_t));
	out->fields = malloc(sizeof(*out->fields) * (scans));
	out->last_scan = RISCV_SCAN_TYPE_INVALID;
	out->read_keys = malloc(sizeof(*out->read_keys) * (scans));
	out->read_keys_used = 0;
	return out;
}

void riscv_batch_free(struct riscv_batch *batch)
{
	free(batch->data_in);
	free(batch->data_out);
	free(batch->fields);
	free(batch);
}

bool riscv_batch_full(struct riscv_batch *batch)
{
	return batch->used_scans > (batch->allocated_scans - 4);
}

void riscv_batch_run(struct riscv_batch *batch)
{
	LOG_DEBUG("running a batch of %ld scans", (long)batch->used_scans);
	riscv_batch_add_nop(batch);

	for (size_t i = 0; i < batch->used_scans; ++i) {
		dump_field(batch->fields + i);
		jtag_add_dr_scan(batch->target->tap, 1, batch->fields + i, TAP_IDLE);
		if (batch->idle_count > 0)
			jtag_add_runtest(batch->idle_count, TAP_IDLE);
	}

	LOG_DEBUG("executing queue");
	if (jtag_execute_queue() != ERROR_OK) {
		LOG_ERROR("Unable to execute JTAG queue");
		abort();
	}

	for (size_t i = 0; i < batch->used_scans; ++i)
		dump_field(batch->fields + i);
}

void riscv_batch_add_dmi_write(struct riscv_batch *batch, unsigned address, uint64_t data)
{
	assert(batch->used_scans < batch->allocated_scans);
	struct scan_field *field = batch->fields + batch->used_scans;
	field->num_bits = riscv_dmi_write_u64_bits(batch->target);
	field->out_value = (void *)(batch->data_out + batch->used_scans * sizeof(uint64_t));
	field->in_value  = (void *)(batch->data_in  + batch->used_scans * sizeof(uint64_t));
	riscv_fill_dmi_write_u64(batch->target, (char *)field->out_value, address, data);
	riscv_fill_dmi_nop_u64(batch->target, (char *)field->in_value);
	batch->last_scan = RISCV_SCAN_TYPE_WRITE;
	batch->used_scans++;
}

size_t riscv_batch_add_dmi_read(struct riscv_batch *batch, unsigned address)
{
	assert(batch->used_scans < batch->allocated_scans);
	struct scan_field *field = batch->fields + batch->used_scans;
	field->num_bits = riscv_dmi_write_u64_bits(batch->target);
	field->out_value = (void *)(batch->data_out + batch->used_scans * sizeof(uint64_t));
	field->in_value  = (void *)(batch->data_in  + batch->used_scans * sizeof(uint64_t));
	riscv_fill_dmi_read_u64(batch->target, (char *)field->out_value, address);
	riscv_fill_dmi_nop_u64(batch->target, (char *)field->in_value);
	batch->last_scan = RISCV_SCAN_TYPE_READ;
	batch->used_scans++;

	/* FIXME We get the read response back on the next scan.  For now I'm
	 * just sticking a NOP in there, but this should be coelesced away. */
	riscv_batch_add_nop(batch);

	batch->read_keys[batch->read_keys_used] = batch->used_scans - 1;
	LOG_DEBUG("read key %ld for batch 0x%p is %ld (0x%p)", batch->read_keys_used, batch, batch->used_scans - 1, (uint64_t*)batch->data_in + (batch->used_scans + 1));
	return batch->read_keys_used++;
}

uint64_t riscv_batch_get_dmi_read(struct riscv_batch *batch, size_t key)
{
	assert(key < batch->read_keys_used);
	size_t index = batch->read_keys[key];
	assert(index <= batch->used_scans);
	uint64_t *addr = ((uint64_t *)(batch->data_in) + index);
	return *addr;
}

void riscv_batch_add_nop(struct riscv_batch *batch)
{
	assert(batch->used_scans < batch->allocated_scans);
	struct scan_field *field = batch->fields + batch->used_scans;
	field->num_bits = riscv_dmi_write_u64_bits(batch->target);
	field->out_value = (void *)(batch->data_out + batch->used_scans * sizeof(uint64_t));
	field->in_value  = (void *)(batch->data_in  + batch->used_scans * sizeof(uint64_t));
	riscv_fill_dmi_nop_u64(batch->target, (char *)field->out_value);
	riscv_fill_dmi_nop_u64(batch->target, (char *)field->in_value);
	batch->last_scan = RISCV_SCAN_TYPE_NOP;
	batch->used_scans++;
	LOG_DEBUG("  added NOP with in_value=0x%p", field->in_value);
}

void dump_field(const struct scan_field *field)
{
        static const char *op_string[] = {"-", "r", "w", "?"};
        static const char *status_string[] = {"+", "?", "F", "b"};

        if (debug_level < LOG_LVL_DEBUG)
                return;

	assert(field->out_value != NULL);
        uint64_t out = buf_get_u64(field->out_value, 0, field->num_bits);
        unsigned int out_op = get_field(out, DTM_DMI_OP);
        unsigned int out_data = get_field(out, DTM_DMI_DATA);
        unsigned int out_address = out >> DTM_DMI_ADDRESS_OFFSET;

        if (field->in_value) {
                uint64_t in = buf_get_u64(field->in_value, 0, field->num_bits);
                unsigned int in_op = get_field(in, DTM_DMI_OP);
                unsigned int in_data = get_field(in, DTM_DMI_DATA);
                unsigned int in_address = in >> DTM_DMI_ADDRESS_OFFSET;

                log_printf_lf(LOG_LVL_DEBUG,
                                __FILE__, __LINE__, __PRETTY_FUNCTION__,
                                "%db %s %08x @%02x -> %s %08x @%02x [0x%p -> 0x%p]",
                                field->num_bits,
                                op_string[out_op], out_data, out_address,
                                status_string[in_op], in_data, in_address,
				field->out_value, field->in_value);
        } else {
                log_printf_lf(LOG_LVL_DEBUG,
                                __FILE__, __LINE__, __PRETTY_FUNCTION__, "%db %s %08x @%02x -> ?",
                                field->num_bits, op_string[out_op], out_data, out_address);
        }
}
