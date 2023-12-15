// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "batch.h"
#include "debug_defines.h"
#include "riscv.h"
#include "field_helpers.h"

#define DTM_DMI_MAX_ADDRESS_LENGTH	((1<<DTM_DTMCS_ABITS_LENGTH)-1)
#define DMI_SCAN_MAX_BIT_LENGTH (DTM_DMI_MAX_ADDRESS_LENGTH + DTM_DMI_DATA_LENGTH + DTM_DMI_OP_LENGTH)
#define DMI_SCAN_BUF_SIZE (DIV_ROUND_UP(DMI_SCAN_MAX_BIT_LENGTH, 8))

/* Reserve extra room in the batch (needed for the last NOP operation) */
#define BATCH_RESERVED_SCANS 1

static void dump_field(int idle, const struct scan_field *field);

typedef struct {
	struct list_head list;
	size_t idle_count;
	size_t until_scan;
} idle_count_info_t;

struct riscv_batch *riscv_batch_alloc(struct target *target, size_t scans, size_t idle_count)
{
	scans += BATCH_RESERVED_SCANS;
	struct riscv_batch *out = calloc(1, sizeof(*out));
	if (!out) {
		LOG_ERROR("Failed to allocate struct riscv_batch");
		return NULL;
	}

	out->target = target;
	out->allocated_scans = scans;
	INIT_LIST_HEAD(&out->idle_counts);
	out->last_scan = RISCV_SCAN_TYPE_INVALID;

	out->data_out = NULL;
	out->data_in = NULL;
	out->fields = NULL;
	out->bscan_ctxt = NULL;
	out->read_keys = NULL;

	idle_count_info_t * const new_entry = malloc(sizeof(*new_entry));
	if (!new_entry) {
		LOG_ERROR("Out of memory!");
		goto alloc_error;
	}
	new_entry->until_scan = scans;
	new_entry->idle_count = idle_count;
	list_add(&new_entry->list, &out->idle_counts);

	/* FIXME: There is potential for memory usage reduction. We could allocate
	 * smaller buffers than DMI_SCAN_BUF_SIZE (that is, buffers that correspond to
	 * the real DR scan length on the given target) */
	out->data_out = malloc(sizeof(*out->data_out) * scans * DMI_SCAN_BUF_SIZE);
	if (!out->data_out) {
		LOG_ERROR("Failed to allocate data_out in RISC-V batch.");
		goto alloc_error;
	};
	out->data_in = malloc(sizeof(*out->data_in) * scans * DMI_SCAN_BUF_SIZE);
	if (!out->data_in) {
		LOG_ERROR("Failed to allocate data_in in RISC-V batch.");
		goto alloc_error;
	}
	out->fields = malloc(sizeof(*out->fields) * scans);
	if (!out->fields) {
		LOG_ERROR("Failed to allocate fields in RISC-V batch.");
		goto alloc_error;
	}
	if (bscan_tunnel_ir_width != 0) {
		out->bscan_ctxt = malloc(sizeof(*out->bscan_ctxt) * scans);
		if (!out->bscan_ctxt) {
			LOG_ERROR("Failed to allocate bscan_ctxt in RISC-V batch.");
			goto alloc_error;
		}
	}
	out->read_keys = malloc(sizeof(*out->read_keys) * scans);
	if (!out->read_keys) {
		LOG_ERROR("Failed to allocate read_keys in RISC-V batch.");
		goto alloc_error;
	}

	return out;

alloc_error:
	riscv_batch_free(out);
	return NULL;
}

void riscv_batch_free(struct riscv_batch *batch)
{
	idle_count_info_t *entry, *tmp;
	list_for_each_entry_safe(entry, tmp, &batch->idle_counts, list)
		free(entry);

	free(batch->data_in);
	free(batch->data_out);
	free(batch->fields);
	free(batch->bscan_ctxt);
	free(batch->read_keys);
	free(batch);
}

bool riscv_batch_full(struct riscv_batch *batch)
{
	return riscv_batch_available_scans(batch) == 0;
}

static void fill_jtag_queue(const struct riscv_batch *batch, size_t start_i)
{
	size_t i = start_i;
	const idle_count_info_t *idle_counts_entry;
	list_for_each_entry(idle_counts_entry, &batch->idle_counts, list) {
		const size_t idle_count = idle_counts_entry->idle_count;
		const size_t until = MIN(batch->used_scans,
				idle_counts_entry->until_scan);
		for (; i < until; ++i) {
			const struct scan_field * const field = batch->fields + i;
			if (bscan_tunnel_ir_width != 0)
				riscv_add_bscan_tunneled_scan(batch->target, field,
						batch->bscan_ctxt + i);
			else
				jtag_add_dr_scan(batch->target->tap, 1, field, TAP_IDLE);

			if (idle_count > 0)
				jtag_add_runtest(idle_count, TAP_IDLE);
		}
	}
	assert(i == batch->used_scans);
}

static void batch_dump_fields(const struct riscv_batch *batch, size_t start_i)
{
	size_t i = start_i;
	const idle_count_info_t *idle_counts_entry;
	list_for_each_entry(idle_counts_entry, &batch->idle_counts, list) {
		const size_t idle_count = idle_counts_entry->idle_count;
		const size_t until = MIN(batch->used_scans,
				idle_counts_entry->until_scan);
		for (; i < until; ++i)
			dump_field(idle_count, batch->fields + i);
	}
	assert(i == batch->used_scans);
}

static int riscv_batch_run_from(struct riscv_batch *batch, size_t start_i)
{
	LOG_TARGET_DEBUG(batch->target, "Running scans [%zu, %zu)",
			start_i, batch->used_scans);

	fill_jtag_queue(batch, start_i);

	keep_alive();

	if (jtag_execute_queue() != ERROR_OK) {
		LOG_TARGET_ERROR(batch->target, "Unable to execute JTAG queue");
		return ERROR_FAIL;
	}

	keep_alive();

	if (bscan_tunnel_ir_width != 0) {
		/* need to right-shift "in" by one bit, because of clock skew between BSCAN TAP and DM TAP */
		for (size_t i = start_i; i < batch->used_scans; ++i) {
			if ((batch->fields + i)->in_value)
				buffer_shr((batch->fields + i)->in_value, DMI_SCAN_BUF_SIZE, 1);
		}
	}

	batch_dump_fields(batch, start_i);
	return ERROR_OK;
}

int riscv_batch_run(struct riscv_batch *batch)
{
	if (batch->used_scans == 0) {
		LOG_TARGET_DEBUG(batch->target, "Ignoring empty batch.");
		return ERROR_OK;
	}
	riscv_batch_add_nop(batch);
	return riscv_batch_run_from(batch, 0);
}

bool riscv_batch_was_busy(const struct riscv_batch *batch, size_t scan_i)
{
	assert(scan_i < batch->used_scans);
	const struct scan_field *field = batch->fields + scan_i;
	const uint64_t in = buf_get_u64(field->in_value, 0, field->num_bits);
	return get_field(in, DTM_DMI_OP) == DTM_DMI_OP_BUSY;
}

size_t riscv_batch_first_busy(const struct riscv_batch *batch)
{
	assert(batch->used_scans);
	assert(riscv_batch_dmi_busy_encountered(batch));
	size_t first_busy = 0;
	while (!riscv_batch_was_busy(batch, first_busy))
		++first_busy;
	return first_busy;
}

int riscv_batch_continue(struct riscv_batch *batch, size_t new_idle)
{
	assert(riscv_batch_dmi_busy_encountered(batch));
	const size_t busy_scan = riscv_batch_first_busy(batch);
	LOG_TARGET_DEBUG(batch->target, "Scan %zu returned busy.", busy_scan);

	size_t old_idle;
	const int result = riscv_batch_change_idle_used_from_scan(batch,
			new_idle, &old_idle, busy_scan);
	if (result != ERROR_OK)
		return result;

	if (new_idle > old_idle)
		jtag_add_runtest(new_idle - old_idle, TAP_IDLE);

	return riscv_batch_run_from(batch, busy_scan);
}

void riscv_batch_add_dm_write(struct riscv_batch *batch, uint64_t address, uint32_t data,
	bool read_back)
{
	assert(batch->used_scans < batch->allocated_scans);
	struct scan_field *field = batch->fields + batch->used_scans;
	field->num_bits = riscv_get_dmi_scan_length(batch->target);
	field->out_value = (void *)(batch->data_out + batch->used_scans * DMI_SCAN_BUF_SIZE);
	riscv_fill_dm_write(batch->target, (char *)field->out_value, address, data);
	if (read_back) {
		field->in_value = (void *)(batch->data_in + batch->used_scans * DMI_SCAN_BUF_SIZE);
		riscv_fill_dm_nop(batch->target, (char *)field->in_value);
	} else {
		field->in_value = NULL;
	}
	batch->last_scan = RISCV_SCAN_TYPE_WRITE;
	batch->used_scans++;
}

size_t riscv_batch_add_dm_read(struct riscv_batch *batch, uint64_t address)
{
	assert(batch->used_scans < batch->allocated_scans);
	struct scan_field *field = batch->fields + batch->used_scans;
	field->num_bits = riscv_get_dmi_scan_length(batch->target);
	field->out_value = (void *)(batch->data_out + batch->used_scans * DMI_SCAN_BUF_SIZE);
	field->in_value  = (void *)(batch->data_in  + batch->used_scans * DMI_SCAN_BUF_SIZE);
	riscv_fill_dm_read(batch->target, (char *)field->out_value, address);
	riscv_fill_dm_nop(batch->target, (char *)field->in_value);
	batch->last_scan = RISCV_SCAN_TYPE_READ;
	batch->used_scans++;

	batch->read_keys[batch->read_keys_used] = batch->used_scans;
	return batch->read_keys_used++;
}

unsigned int riscv_batch_get_dmi_read_op(const struct riscv_batch *batch, size_t key)
{
	assert(key < batch->read_keys_used);
	size_t index = batch->read_keys[key];
	assert(index < batch->used_scans);
	uint8_t *base = batch->data_in + DMI_SCAN_BUF_SIZE * index;
	/* extract "op" field from the DMI read result */
	return (unsigned int)buf_get_u32(base, DTM_DMI_OP_OFFSET, DTM_DMI_OP_LENGTH);
}

uint32_t riscv_batch_get_dmi_read_data(const struct riscv_batch *batch, size_t key)
{
	assert(key < batch->read_keys_used);
	size_t index = batch->read_keys[key];
	assert(index < batch->used_scans);
	uint8_t *base = batch->data_in + DMI_SCAN_BUF_SIZE * index;
	/* extract "data" field from the DMI read result */
	return buf_get_u32(base, DTM_DMI_DATA_OFFSET, DTM_DMI_DATA_LENGTH);
}

void riscv_batch_add_nop(struct riscv_batch *batch)
{
	assert(batch->used_scans < batch->allocated_scans);
	struct scan_field *field = batch->fields + batch->used_scans;
	field->num_bits = riscv_get_dmi_scan_length(batch->target);
	field->out_value = (void *)(batch->data_out + batch->used_scans * DMI_SCAN_BUF_SIZE);
	field->in_value  = (void *)(batch->data_in  + batch->used_scans * DMI_SCAN_BUF_SIZE);
	riscv_fill_dm_nop(batch->target, (char *)field->out_value);
	riscv_fill_dm_nop(batch->target, (char *)field->in_value);
	batch->last_scan = RISCV_SCAN_TYPE_NOP;
	batch->used_scans++;
}

static void dump_field(int idle, const struct scan_field *field)
{
	static const char * const op_string[] = {"-", "r", "w", "?"};
	static const char * const status_string[] = {"+", "?", "F", "b"};

	if (debug_level < LOG_LVL_DEBUG)
		return;

	assert(field->out_value);
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
				__FILE__, __LINE__, __func__,
				"%db %s %08x @%02x -> %s %08x @%02x; %di",
				field->num_bits, op_string[out_op], out_data, out_address,
				status_string[in_op], in_data, in_address, idle);
	} else {
		log_printf_lf(LOG_LVL_DEBUG,
				__FILE__, __LINE__, __func__, "%db %s %08x @%02x -> ?; %di",
				field->num_bits, op_string[out_op], out_data, out_address, idle);
	}
}

size_t riscv_batch_available_scans(struct riscv_batch *batch)
{
	assert(batch->allocated_scans >= (batch->used_scans + BATCH_RESERVED_SCANS));
	return batch->allocated_scans - batch->used_scans - BATCH_RESERVED_SCANS;
}

bool riscv_batch_dmi_busy_encountered(const struct riscv_batch *batch)
{
	if (batch->used_scans == 0)
		/* Empty batch */
		return false;

	assert(batch->last_scan == RISCV_SCAN_TYPE_NOP);
	return riscv_batch_was_busy(batch, batch->used_scans - 1);
}

/* Returns the entry in `batch->idle_counts` that defines idle_count for the scan. */
static idle_count_info_t *find_idle_counts_entry(struct riscv_batch *batch, size_t scan_idx)
{
	assert(!list_empty(&batch->idle_counts));

	idle_count_info_t *entry;
	list_for_each_entry(entry, &batch->idle_counts, list)
		if (entry->until_scan > scan_idx)
			break;
	assert(!list_entry_is_head(entry, &batch->idle_counts, list));
	return entry;
}

int riscv_batch_change_idle_used_from_scan(struct riscv_batch *batch, size_t new_idle,
		size_t *old_idle, size_t scan_idx)
{
	idle_count_info_t * const new_entry = malloc(sizeof(*new_entry));
	if (!new_entry) {
		LOG_ERROR("Out of memory!");
		return ERROR_FAIL;
	}
	new_entry->until_scan = scan_idx;
	idle_count_info_t *old_entry = find_idle_counts_entry(batch, scan_idx);
	/* Add new entry before the old one. */
	list_add_tail(&new_entry->list, &old_entry->list);
	assert(new_entry->until_scan < old_entry->until_scan);
	/* new entry now defines the range until the scan (non-inclusive) */
	new_entry->idle_count = old_entry->idle_count;
	/* old entry now defines the range from the scan (inclusive) */
	if (old_idle)
		*old_idle = old_entry->idle_count;
	old_entry->idle_count = new_idle;
	LOG_DEBUG("Will use idle == %zu from scan %zu until scan %zu.", old_entry->idle_count,
			new_entry->until_scan, old_entry->until_scan);
	return ERROR_OK;
}
