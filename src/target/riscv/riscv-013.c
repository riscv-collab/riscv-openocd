/*
 * Support for RISC-V, debug version 0.13, which is currently (2/4/17) the
 * latest draft.
 */

#include <assert.h>
#include <stdlib.h>
#include <time.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "target.h"
#include "target/algorithm.h"
#include "target_type.h"
#include "log.h"
#include "jtag/jtag.h"
#include "register.h"
#include "breakpoints.h"
#include "helper/time_support.h"
#include "riscv.h"
#include "debug_defines.h"
#include "rtos/rtos.h"

static void riscv013_on_step_or_resume(struct target *target, bool step);
static void riscv013_step_or_resume_current_hart(struct target *target, bool step);

/* Implementations of the functions in riscv_info_t. */
static riscv_reg_t riscv013_get_register(struct target *target, int hartid, int regid);
static void riscv013_set_register(struct target *target, int hartid, int regid, uint64_t value);
static void riscv013_select_current_hart(struct target *target);
static void riscv013_halt_current_hart(struct target *target);
static void riscv013_resume_current_hart(struct target *target);
static void riscv013_step_current_hart(struct target *target);
static void riscv013_on_halt(struct target *target);
static void riscv013_on_step(struct target *target);
static void riscv013_on_resume(struct target *target);
static bool riscv013_is_halted(struct target *target);
static enum riscv_halt_reason riscv013_halt_reason(struct target *target);

/**
 * Since almost everything can be accomplish by scanning the dbus register, all
 * functions here assume dbus is already selected. The exception are functions
 * called directly by OpenOCD, which can't assume anything about what's
 * currently in IR. They should set IR to dbus explicitly.
 */

#define get_field(reg, mask) (((reg) & (mask)) / ((mask) & ~((mask) << 1)))
#define set_field(reg, mask, val) (((reg) & ~(mask)) | (((val) * ((mask) & ~((mask) << 1))) & (mask)))

#define DIM(x)		(sizeof(x)/sizeof(*x))

#define CSR_DCSR_CAUSE_SWBP		1
#define CSR_DCSR_CAUSE_TRIGGER	2
#define CSR_DCSR_CAUSE_DEBUGINT	3
#define CSR_DCSR_CAUSE_STEP		4
#define CSR_DCSR_CAUSE_HALT		5

/*** JTAG registers. ***/

typedef enum {
	DMI_OP_NOP = 0,
	DMI_OP_READ = 1,
	DMI_OP_WRITE = 2
} dmi_op_t;
typedef enum {
	DMI_STATUS_SUCCESS = 0,
	DMI_STATUS_FAILED = 2,
	DMI_STATUS_BUSY = 3
} dmi_status_t;

typedef enum {
	RE_OK,
	RE_FAIL,
	RE_AGAIN
} riscv_error_t;

typedef enum slot {
	SLOT0,
	SLOT1,
	SLOT_LAST,
} slot_t;

/*** Debug Bus registers. ***/

#define CMDERR_NONE				0
#define CMDERR_BUSY				1
#define CMDERR_NOT_SUPPORTED	2
#define CMDERR_EXCEPTION		3
#define CMDERR_HALT_RESUME		4
#define CMDERR_OTHER			7

/*** Info about the core being debugged. ***/

#define WALL_CLOCK_TIMEOUT		2

#define MAX_HWBPS			16

struct trigger {
	uint64_t address;
	uint32_t length;
	uint64_t mask;
	uint64_t value;
	bool read, write, execute;
	int unique_id;
};

struct memory_cache_line {
	uint32_t data;
	bool valid;
	bool dirty;
};

typedef struct {
	/* Number of address bits in the dbus register. */
	unsigned abits;
	/* Number of abstract command data registers. */
	unsigned datacount;
	/* Number of words in the Program Buffer. */
	unsigned progsize;
	/* Number of Program Buffer registers. */
	/* Number of words in Debug RAM. */
	uint64_t misa;
	uint64_t tselect;
	bool tselect_dirty;
	/* The value that mstatus actually has on the target right now. This is not
	 * the value we present to the user. That one may be stored in the
	 * reg_cache. */
	uint64_t mstatus_actual;

	/* Single buffer that contains all register names, instead of calling
	 * malloc for each register. Needs to be freed when reg_list is freed. */
	char *reg_names;
	/* Single buffer that contains all register values. */
	void *reg_values;

	// For each physical trigger, contains -1 if the hwbp is available, or the
	// unique_id of the breakpoint/watchpoint that is using it.
	int trigger_unique_id[MAX_HWBPS];

	unsigned int trigger_count;

	// Number of run-test/idle cycles the target requests we do after each dbus
	// access.
	unsigned int dtmcontrol_idle;

	// This value is incremented every time a dbus access comes back as "busy".
	// It's used to determine how many run-test/idle cycles to feed the target
	// in between accesses.
	unsigned int dmi_busy_delay;

	// This value is increased every time we tried to execute two commands
	// consecutively, and the second one failed because the previous hadn't
	// completed yet.  It's used to add extra run-test/idle cycles after
	// starting a command, so we don't have to waste time checking for busy to
	// go low.
	unsigned int ac_busy_delay;

	bool need_strict_step;
} riscv013_info_t;

static void dump_field(const struct scan_field *field)
{
	static const char *op_string[] = {"-", "r", "w", "?"};
	static const char *status_string[] = {"+", "?", "F", "b"};

	if (debug_level < LOG_LVL_DEBUG)
		return;

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
				__FILE__, __LINE__, "scan",
				"%db %s %08x @%02x -> %s %08x @%02x",
				field->num_bits,
				op_string[out_op], out_data, out_address,
				status_string[in_op], in_data, in_address);
	} else {
		log_printf_lf(LOG_LVL_DEBUG,
				__FILE__, __LINE__, "scan", "%db %s %08x @%02x -> ?",
				field->num_bits, op_string[out_op], out_data, out_address);
	}
}

static riscv013_info_t *get_info(const struct target *target)
{
	riscv_info_t *info = (riscv_info_t *) target->arch_info;
	return (riscv013_info_t *) info->version_specific;
}

/*** Necessary prototypes. ***/

static int register_get(struct reg *reg);

/*** Utility functions. ***/

bool supports_extension(struct target *target, char letter)
{
	riscv013_info_t *info = get_info(target);
	unsigned num;
	if (letter >= 'a' && letter <= 'z') {
		num = letter - 'a';
	} else if (letter >= 'A' && letter <= 'Z') {
		num = letter - 'A';
	} else {
		return false;
	}
	return info->misa & (1 << num);
}

static void select_dmi(struct target *target)
{
	static uint8_t ir_dmi[1] = {DTM_DMI};
	struct scan_field field = {
		.num_bits = target->tap->ir_length,
		.out_value = ir_dmi,
		.in_value = NULL,
		.check_value = NULL,
		.check_mask = NULL
	};

	jtag_add_ir_scan(target->tap, &field, TAP_IDLE);
}

static uint32_t dtmcontrol_scan(struct target *target, uint32_t out)
{
	struct scan_field field;
	uint8_t in_value[4];
	uint8_t out_value[4];

	buf_set_u32(out_value, 0, 32, out);

	jtag_add_ir_scan(target->tap, &select_dtmcontrol, TAP_IDLE);

	field.num_bits = 32;
	field.out_value = out_value;
	field.in_value = in_value;
	jtag_add_dr_scan(target->tap, 1, &field, TAP_IDLE);

	/* Always return to dmi. */
	select_dmi(target);

	int retval = jtag_execute_queue();
	if (retval != ERROR_OK) {
		LOG_ERROR("failed jtag scan: %d", retval);
		return retval;
	}

	uint32_t in = buf_get_u32(field.in_value, 0, 32);
	LOG_DEBUG("DTMCS: 0x%x -> 0x%x", out, in);

	return in;
}

static void increase_dmi_busy_delay(struct target *target)
{
	riscv013_info_t *info = get_info(target);
	info->dmi_busy_delay += info->dmi_busy_delay / 10 + 1;
	LOG_INFO("dtmcontrol_idle=%d, dmi_busy_delay=%d, ac_busy_delay=%d",
			info->dtmcontrol_idle, info->dmi_busy_delay,
			info->ac_busy_delay);

	dtmcontrol_scan(target, DTM_DTMCS_DMIRESET);
}

/**
 * exec: If this is set, assume the scan results in an execution, so more
 * run-test/idle cycles may be required.
 */
static dmi_status_t dmi_scan(struct target *target, uint16_t *address_in,
		uint64_t *data_in, dmi_op_t op, uint16_t address_out, uint64_t data_out,
		bool exec)
{
	riscv013_info_t *info = get_info(target);
	uint8_t in[8] = {0};
	uint8_t out[8];
	struct scan_field field = {
		.num_bits = info->abits + DTM_DMI_OP_LENGTH + DTM_DMI_DATA_LENGTH,
		.out_value = out,
	};

	if (address_in || data_in) {
		field.in_value = in;
	}

	assert(info->abits != 0);

	buf_set_u64(out, DTM_DMI_OP_OFFSET, DTM_DMI_OP_LENGTH, op);
	buf_set_u64(out, DTM_DMI_DATA_OFFSET, DTM_DMI_DATA_LENGTH, data_out);
	buf_set_u64(out, DTM_DMI_ADDRESS_OFFSET, info->abits, address_out);

	/* Assume dbus is already selected. */
	jtag_add_dr_scan(target->tap, 1, &field, TAP_IDLE);

	int idle_count = info->dtmcontrol_idle + info->dmi_busy_delay;
	if (exec)
		idle_count += info->ac_busy_delay;

	if (idle_count) {
		jtag_add_runtest(idle_count, TAP_IDLE);
	}

	int retval = jtag_execute_queue();
	if (retval != ERROR_OK) {
		LOG_ERROR("dmi_scan failed jtag scan");
		return DMI_STATUS_FAILED;
	}

	if (data_in) {
		*data_in = buf_get_u64(in, DTM_DMI_DATA_OFFSET, DTM_DMI_DATA_LENGTH);
	}

	if (address_in) {
		*address_in = buf_get_u32(in, DTM_DMI_ADDRESS_OFFSET, info->abits);
	}

	dump_field(&field);

	return buf_get_u32(in, DTM_DMI_OP_OFFSET, DTM_DMI_OP_LENGTH);
}

static uint64_t dmi_read(struct target *target, uint16_t address)
{
	select_dmi(target);

	uint64_t value;
	dmi_status_t status;
	uint16_t address_in;

	unsigned i = 0;
	for (i = 0; i < 256; i++) {
		status = dmi_scan(target, NULL, NULL, DMI_OP_READ, address, 0,
				false);
		if (status == DMI_STATUS_BUSY) {
			increase_dmi_busy_delay(target);
		} else {
			break;
		}
	}

	status = dmi_scan(target, &address_in, &value, DMI_OP_NOP, address, 0,
			false);

	if (status != DMI_STATUS_SUCCESS) {
		LOG_ERROR("failed read from 0x%x; value=0x%" PRIx64 ", status=%d\n",
				address, value, status);
	}

	return value;
}

static void dmi_write(struct target *target, uint16_t address, uint64_t value)
{
	select_dmi(target);
	dmi_status_t status = DMI_STATUS_BUSY;
	unsigned i = 0;
	while (status == DMI_STATUS_BUSY && i++ < 256) {
		dmi_scan(target, NULL, NULL, DMI_OP_WRITE, address, value,
				address == DMI_COMMAND);
		status = dmi_scan(target, NULL, NULL, DMI_OP_NOP, 0, 0, false);
		if (status == DMI_STATUS_BUSY) {
			increase_dmi_busy_delay(target);
		}
	}
	if (status != DMI_STATUS_SUCCESS) {
		LOG_ERROR("failed to write 0x%" PRIx64 " to 0x%x; status=%d\n", value, address, status);
	}
}

/** Convert register number (internal OpenOCD number) to the number expected by
 * the abstract command interface. */
static unsigned reg_number_to_no(unsigned reg_num)
{
	if (reg_num <= GDB_REGNO_XPR31) {
		return reg_num + 0x1000 - GDB_REGNO_XPR0;
	} else if (reg_num >= GDB_REGNO_CSR0 && reg_num <= GDB_REGNO_CSR4095) {
		return reg_num - GDB_REGNO_CSR0;
	} else if (reg_num >= GDB_REGNO_FPR0 && reg_num <= GDB_REGNO_FPR31) {
		return reg_num + 0x1020 - GDB_REGNO_FPR0;
	} else {
		return ~0;
	}
}

uint32_t abstract_register_size(unsigned width)
{
	switch (width) {
		case 32:
			return set_field(0, AC_ACCESS_REGISTER_SIZE, 2);
		case 64:
			return set_field(0, AC_ACCESS_REGISTER_SIZE, 3);
			break;
		case 128:
			return set_field(0, AC_ACCESS_REGISTER_SIZE, 4);
			break;
		default:
			LOG_ERROR("Unsupported register width: %d", width);
			return 0;
	}
}

static int wait_for_idle(struct target *target, uint32_t *abstractcs)
{
	time_t start = time(NULL);
	while (1) {
		*abstractcs = dmi_read(target, DMI_ABSTRACTCS);

		if (get_field(*abstractcs, DMI_ABSTRACTCS_BUSY) == 0) {
			return ERROR_OK;
		}

		if (time(NULL) - start > WALL_CLOCK_TIMEOUT) {
			if (get_field(*abstractcs, DMI_ABSTRACTCS_CMDERR) != CMDERR_NONE) {
				const char *errors[8] = {
					"none",
					"busy",
					"not supported",
					"exception",
					"halt/resume",
					"reserved",
					"reserved",
					"other" };

				LOG_ERROR("Abstract command ended in error '%s' (abstractcs=0x%x)",
						errors[get_field(*abstractcs, DMI_ABSTRACTCS_CMDERR)],
						*abstractcs);
			}

			LOG_ERROR("Timed out waiting for busy to go low. (abstractcs=0x%x)",
					*abstractcs);
			return ERROR_FAIL;
		}
	}
}

static int execute_abstract_command(struct target *target, uint32_t command)
{
	dmi_write(target, DMI_COMMAND, command);

	uint32_t abstractcs;
	if (wait_for_idle(target, &abstractcs) != ERROR_OK)
		return ERROR_FAIL;

	if (get_field(abstractcs, DMI_ABSTRACTCS_CMDERR) != CMDERR_NONE) {
		const char *errors[8] = {
			"none",
			"busy",
			"not supported",
			"exception",
			"halt/resume",
			"reserved",
			"reserved",
			"other" };
		LOG_DEBUG("Abstract command 0x%x ended in error '%s' (abstractcs=0x%x)",
				command, errors[get_field(abstractcs, DMI_ABSTRACTCS_CMDERR)],
				abstractcs);
		// Clear the error.
		dmi_write(target, DMI_ABSTRACTCS, DMI_ABSTRACTCS_CMDERR);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

/*** program "class" ***/
/* This class allows a debug program to be built up piecemeal, and then be
 * executed. If necessary, the program is split up to fit in the program
 * buffer. */

typedef struct {
	uint8_t code[12 * 4];
	unsigned length;
	bool write;
	unsigned regno;
	uint64_t write_value;
} program_t;

static void program_add32(program_t *program, uint32_t instruction);

static program_t *program_new(void)
{
	program_t *program = malloc(sizeof(program_t));
	if (program) {
		program->length = 0;
		// Default to read zero.
		program->write = false;
		program->regno = 0x1000;
	}
	program_add32(program, fence_i());
	return program;
}

static void program_delete(program_t *program)
{
	free(program);
}

static void program_add32(program_t *program, uint32_t instruction)
{
	assert(program->length + 4 < sizeof(program->code));
	program->code[program->length++] = instruction & 0xff;
	program->code[program->length++] = (instruction >> 8) & 0xff;
	program->code[program->length++] = (instruction >> 16) & 0xff;
	program->code[program->length++] = (instruction >> 24) & 0xff;
}

static void program_set_read(program_t *program, unsigned reg_num)
{
	program->write = false;
	program->regno = reg_number_to_no(reg_num);
}

static void program_set_write(program_t *program, unsigned reg_num, uint64_t value)
{
	program->write = true;
	program->regno = reg_number_to_no(reg_num);
	program->write_value = value;
}

/*** end of program class ***/

static void write_program(struct target *target, const program_t *program)
{
	riscv013_info_t *info = get_info(target);

	assert(program->length <= info->progsize * 4);
	for (unsigned i = 0; i < program->length; i += 4) {
		uint32_t value =
			program->code[i] |
			((uint32_t) program->code[i+1] << 8) |
			((uint32_t) program->code[i+2] << 16) |
			((uint32_t) program->code[i+3] << 24);
		dmi_write(target, DMI_PROGBUF0 + i / 4, value);
	}
}

static int execute_program(struct target *target, const program_t *program)
{
	write_program(target, program);

	uint32_t command = 0;
	if (program->write) {
		if (get_field(command, AC_ACCESS_REGISTER_SIZE) > 2) {
			dmi_write(target, DMI_DATA1, program->write_value >> 32);
		}
		dmi_write(target, DMI_DATA0, program->write_value);
		command |= AC_ACCESS_REGISTER_WRITE | AC_ACCESS_REGISTER_POSTEXEC;
	} else {
		command |= AC_ACCESS_REGISTER_PREEXEC;
	}
	command |= abstract_register_size(riscv_xlen(target));
	command |= program->regno;

	return execute_abstract_command(target, command);
}

static int abstract_read_register(struct target *target,
		uint64_t *value,
		uint32_t reg_number, 
		unsigned width)
{
	uint32_t command = abstract_register_size(width);

	command |= reg_number_to_no(reg_number);

	int result = execute_abstract_command(target, command);
	if (result != ERROR_OK) {
		return result;
	}

	if (value) {
		*value = 0;
		switch (width) {
			case 128:
				LOG_ERROR("Ignoring top 64 bits from 128-bit register read.");
			case 64:
				*value |= ((uint64_t) dmi_read(target, DMI_DATA1)) << 32;
			case 32:
				*value |= dmi_read(target, DMI_DATA0);
				break;
		}
	}

	return ERROR_OK;
}

static int abstract_write_register(struct target *target,
		unsigned reg_number, 
		unsigned width,
		uint64_t value)
{
	uint32_t command = abstract_register_size(width);

	command |= reg_number_to_no(reg_number);
	command |= AC_ACCESS_REGISTER_WRITE;

	switch (width) {
		case 128:
			LOG_ERROR("Ignoring top 64 bits from 128-bit register write.");
		case 64:
			dmi_write(target, DMI_DATA1, value >> 32);
		case 32:
			dmi_write(target, DMI_DATA0, value);
			break;
	}

	int result = execute_abstract_command(target, command);
	if (result != ERROR_OK) {
		return result;
	}

	return ERROR_OK;
}

static int update_mstatus_actual(struct target *target)
{
	struct reg *mstatus_reg = &target->reg_cache->reg_list[GDB_REGNO_MSTATUS];
	if (mstatus_reg->valid) {
		// We previously made it valid.
		return ERROR_OK;
	}

	LOG_DEBUG("Reading mstatus");

	// Force reading the register. In that process mstatus_actual will be
	// updated.
	return register_get(&target->reg_cache->reg_list[GDB_REGNO_MSTATUS]);
}

static int register_write_direct(struct target *target, unsigned number,
		uint64_t value)
{
	riscv013_info_t *info = get_info(target);
	LOG_DEBUG("register 0x%x <- 0x%" PRIx64, number, value);

	if (number == GDB_REGNO_MSTATUS) {
		info->mstatus_actual = value;
	}

	int result = abstract_write_register(target, number, riscv_xlen(target), value);
	if (result == ERROR_OK)
		return result;

	// Fall back to program buffer.
	if (number >= GDB_REGNO_FPR0 && number <= GDB_REGNO_FPR31) {
		result = update_mstatus_actual(target);
		if (result != ERROR_OK) {
			return result;
		}
		if ((info->mstatus_actual & MSTATUS_FS) == 0) {
			result = register_write_direct(target, GDB_REGNO_MSTATUS, 
					set_field(info->mstatus_actual, MSTATUS_FS, 1));
			if (result != ERROR_OK)
				return result;
		}

		program_t *program = program_new();
		// TODO: Fully support D extension on RV32.
		if (supports_extension(target, 'D') && riscv_xlen(target) >= 64) {
			program_add32(program, fmv_d_x(number - GDB_REGNO_FPR0, S0));
		} else {
			program_add32(program, fmv_s_x(number - GDB_REGNO_FPR0, S0));
		}
		program_add32(program, ebreak());
		program_set_write(program, S0, value);
		result = execute_program(target, program);
		program_delete(program);
	} else if (number >= GDB_REGNO_CSR0 && number <= GDB_REGNO_CSR4095) {
		program_t *program = program_new();
		program_add32(program, csrw(S0, number - GDB_REGNO_CSR0));
		program_add32(program, ebreak());
		program_set_write(program, S0, value);
		result = execute_program(target, program);
		program_delete(program);
	} else {
		return result;
	}

	return result;
}

/** Actually read registers from the target right now. */
static int register_read_direct(struct target *target, uint64_t *value, uint32_t number)
{
	riscv013_info_t *info = get_info(target);
	int result = abstract_read_register(target, value, number, riscv_xlen(target));
	if (result == ERROR_OK)
		return result;

	// Fall back to program buffer.
	if (number >= GDB_REGNO_FPR0 && number <= GDB_REGNO_FPR31) {
		result = update_mstatus_actual(target);
		if (result != ERROR_OK) {
			return result;
		}
		if ((info->mstatus_actual & MSTATUS_FS) == 0) {
			result = register_write_direct(target, GDB_REGNO_MSTATUS,
					set_field(info->mstatus_actual, MSTATUS_FS, 1));
			if (result != ERROR_OK)
				return result;
		}
		LOG_DEBUG("mstatus_actual=0x%lx", info->mstatus_actual);

		program_t *program = program_new();
		if (supports_extension(target, 'D') && riscv_xlen(target) >= 64) {
			program_add32(program, fmv_x_d(S0, number - GDB_REGNO_FPR0));
		} else {
			program_add32(program, fmv_x_s(S0, number - GDB_REGNO_FPR0));
		}
		program_add32(program, ebreak());
		program_set_read(program, S0);
		result = execute_program(target, program);
		program_delete(program);
	} else if (number >= GDB_REGNO_CSR0 && number <= GDB_REGNO_CSR4095) {
		program_t *program = program_new();
		program_add32(program, csrr(S0, number - GDB_REGNO_CSR0));
		program_add32(program, ebreak());
		program_set_read(program, S0);
		result = execute_program(target, program);
		program_delete(program);
	} else {
		return result;
	}

	if (result != ERROR_OK)
		return result;

	result = register_read_direct(target, value, S0);
	if (result != ERROR_OK)
		return result;

	LOG_DEBUG("register 0x%x = 0x%" PRIx64, number, *value);

	return ERROR_OK;
}

static int maybe_read_tselect(struct target *target)
{
	riscv013_info_t *info = get_info(target);

	if (info->tselect_dirty) {
		int result = register_read_direct(target, &info->tselect, GDB_REGNO_TSELECT);
		if (result != ERROR_OK)
			return result;
		info->tselect_dirty = false;
	}

	return ERROR_OK;
}

static int maybe_write_tselect(struct target *target)
{
	riscv013_info_t *info = get_info(target);

	if (!info->tselect_dirty) {
		int result = register_write_direct(target, GDB_REGNO_TSELECT, info->tselect);
		if (result != ERROR_OK)
			return result;
		info->tselect_dirty = true;
	}

	return ERROR_OK;
}

/*** OpenOCD target functions. ***/

static int register_get(struct reg *reg)
{
	struct target *target = (struct target *) reg->arch_info;
	riscv013_info_t *info = get_info(target);

	maybe_write_tselect(target);

	if (reg->number <= GDB_REGNO_XPR31) {
		register_read_direct(target, reg->value, reg->number);
		return ERROR_OK;
	} else if (reg->number == GDB_REGNO_PC) {
		buf_set_u32(reg->value, 0, 32, riscv_peek_register(target, GDB_REGNO_DPC));
		reg->valid = true;
		return ERROR_OK;
	} else if (reg->number == GDB_REGNO_PRIV) {
		uint64_t dcsr = riscv_peek_register(target, CSR_DCSR);
		buf_set_u64(reg->value, 0, 8, get_field(dcsr, CSR_DCSR_PRV));
		riscv_overwrite_register(target, CSR_DCSR, dcsr);
		return ERROR_OK;
	} else {
		uint64_t value;
		int result = register_read_direct(target, &value, reg->number);
		if (result != ERROR_OK) {
			return result;
		}
		LOG_DEBUG("%s=0x%" PRIx64, reg->name, value);
		buf_set_u64(reg->value, 0, riscv_xlen(target), value);

		if (reg->number == GDB_REGNO_MSTATUS) {
			info->mstatus_actual = value;
			reg->valid = true;
		}
	}

	return ERROR_OK;
}

static int register_write(struct target *target, unsigned int number,
		uint64_t value)
{
	maybe_write_tselect(target);

	if (number == GDB_REGNO_PC) {
		riscv_overwrite_register(target, GDB_REGNO_DPC, value);
	} else if (number == GDB_REGNO_PRIV) {
		uint64_t dcsr = riscv_peek_register(target, CSR_DCSR);
		dcsr = set_field(dcsr, CSR_DCSR_PRV, value);
		riscv_overwrite_register(target, GDB_REGNO_DCSR, dcsr);
	} else {
		return register_write_direct(target, number, value);
	}

	return ERROR_OK;
}

static int register_set(struct reg *reg, uint8_t *buf)
{
	struct target *target = (struct target *) reg->arch_info;

	uint64_t value = buf_get_u64(buf, 0, riscv_xlen(target));

	LOG_DEBUG("write 0x%" PRIx64 " to %s", value, reg->name);
	struct reg *r = &target->reg_cache->reg_list[reg->number];
	r->valid = true;
	memcpy(r->value, buf, (r->size + 7) / 8);

	return register_write(target, reg->number, value);
}

static struct reg_arch_type riscv_reg_arch_type = {
	.get = register_get,
	.set = register_set
};

static int init_target(struct command_context *cmd_ctx,
		struct target *target)
{
	LOG_DEBUG("init");
	riscv_info_t *generic_info = (riscv_info_t *) target->arch_info;

	riscv_info_init(generic_info);
	generic_info->get_register = &riscv013_get_register;
	generic_info->set_register = &riscv013_set_register;
	generic_info->select_current_hart = &riscv013_select_current_hart;
	generic_info->is_halted = &riscv013_is_halted;
	generic_info->halt_current_hart = &riscv013_halt_current_hart;
	generic_info->resume_current_hart = &riscv013_resume_current_hart;
	generic_info->step_current_hart = &riscv013_step_current_hart;
	generic_info->on_halt = &riscv013_on_halt;
	generic_info->on_resume = &riscv013_on_resume;
	generic_info->on_step = &riscv013_on_step;
	generic_info->halt_reason = &riscv013_halt_reason;

	generic_info->version_specific = calloc(1, sizeof(riscv013_info_t));
	if (!generic_info->version_specific)
		return ERROR_FAIL;
	riscv013_info_t *info = get_info(target);

	target->reg_cache = calloc(1, sizeof(*target->reg_cache));
	target->reg_cache->name = "RISC-V registers";
	target->reg_cache->num_regs = GDB_REGNO_COUNT;

	target->reg_cache->reg_list = calloc(GDB_REGNO_COUNT, sizeof(struct reg));

	const unsigned int max_reg_name_len = 12;
	info->reg_names = calloc(1, GDB_REGNO_COUNT * max_reg_name_len);
	char *reg_name = info->reg_names;
	info->reg_values = NULL;

	for (unsigned int i = 0; i < GDB_REGNO_COUNT; i++) {
		struct reg *r = &target->reg_cache->reg_list[i];
		r->number = i;
		r->caller_save = true;
		r->dirty = false;
		r->valid = false;
		r->exist = true;
		r->type = &riscv_reg_arch_type;
		r->arch_info = target;
		if (i <= GDB_REGNO_XPR31) {
			sprintf(reg_name, "x%d", i);
		} else if (i == GDB_REGNO_PC) {
			sprintf(reg_name, "pc");
		} else if (i >= GDB_REGNO_FPR0 && i <= GDB_REGNO_FPR31) {
			sprintf(reg_name, "f%d", i - GDB_REGNO_FPR0);
		} else if (i >= GDB_REGNO_CSR0 && i <= GDB_REGNO_CSR4095) {
			sprintf(reg_name, "csr%d", i - GDB_REGNO_CSR0);
		} else if (i == GDB_REGNO_PRIV) {
			sprintf(reg_name, "priv");
		}
		if (reg_name[0]) {
			r->name = reg_name;
		}
		reg_name += strlen(reg_name) + 1;
		assert(reg_name < info->reg_names + GDB_REGNO_COUNT * max_reg_name_len);
	}
#if 0
	update_reg_list(target);
#endif

	memset(info->trigger_unique_id, 0xff, sizeof(info->trigger_unique_id));

	return ERROR_OK;
}

static void deinit_target(struct target *target)
{
	LOG_DEBUG("riscv_deinit_target()");
	riscv_info_t *info = (riscv_info_t *) target->arch_info;
	free(info->version_specific);
	info->version_specific = NULL;
}

static int add_trigger(struct target *target, struct trigger *trigger)
{
	riscv013_info_t *info = get_info(target);

	maybe_read_tselect(target);

	unsigned int i;
	for (i = 0; i < info->trigger_count; i++) {
		if (info->trigger_unique_id[i] != -1) {
			continue;
		}

		register_write_direct(target, GDB_REGNO_TSELECT, i);

		uint64_t tdata1;
		register_read_direct(target, &tdata1, GDB_REGNO_TDATA1);
		int type = get_field(tdata1, MCONTROL_TYPE(riscv_xlen(target)));

		if (type != 2) {
			continue;
		}

		if (tdata1 & (MCONTROL_EXECUTE | MCONTROL_STORE | MCONTROL_LOAD)) {
			// Trigger is already in use, presumably by user code.
			continue;
		}

		// address/data match trigger
		tdata1 |= MCONTROL_DMODE(riscv_xlen(target));
		tdata1 = set_field(tdata1, MCONTROL_ACTION,
				MCONTROL_ACTION_DEBUG_MODE);
		tdata1 = set_field(tdata1, MCONTROL_MATCH, MCONTROL_MATCH_EQUAL);
		tdata1 |= MCONTROL_M;
		if (info->misa & (1 << ('H' - 'A')))
			tdata1 |= MCONTROL_H;
		if (info->misa & (1 << ('S' - 'A')))
			tdata1 |= MCONTROL_S;
		if (info->misa & (1 << ('U' - 'A')))
			tdata1 |= MCONTROL_U;

		if (trigger->execute)
			tdata1 |= MCONTROL_EXECUTE;
		if (trigger->read)
			tdata1 |= MCONTROL_LOAD;
		if (trigger->write)
			tdata1 |= MCONTROL_STORE;

		register_write_direct(target, GDB_REGNO_TDATA1, tdata1);

		uint64_t tdata1_rb;
		register_read_direct(target, &tdata1_rb, GDB_REGNO_TDATA1);
		LOG_DEBUG("tdata1=0x%" PRIx64, tdata1_rb);

		if (tdata1 != tdata1_rb) {
			LOG_DEBUG("Trigger %d doesn't support what we need; After writing 0x%"
					PRIx64 " to tdata1 it contains 0x%" PRIx64,
					i, tdata1, tdata1_rb);
			register_write_direct(target, GDB_REGNO_TDATA1, 0);
			continue;
		}

		register_write_direct(target, GDB_REGNO_TDATA2, trigger->address);

		LOG_DEBUG("Using resource %d for bp %d", i,
				trigger->unique_id);
		info->trigger_unique_id[i] = trigger->unique_id;
		break;
	}
	if (i >= info->trigger_count) {
		LOG_ERROR("Couldn't find an available hardware trigger.");
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}

	return ERROR_OK;
}

static int remove_trigger(struct target *target, struct trigger *trigger)
{
	riscv013_info_t *info = get_info(target);

	maybe_read_tselect(target);

	unsigned int i;
	for (i = 0; i < info->trigger_count; i++) {
		if (info->trigger_unique_id[i] == trigger->unique_id) {
			break;
		}
	}
	if (i >= info->trigger_count) {
		LOG_ERROR("Couldn't find the hardware resources used by hardware "
				"trigger.");
		return ERROR_FAIL;
	}
	LOG_DEBUG("Stop using resource %d for bp %d", i, trigger->unique_id);
	register_write_direct(target, GDB_REGNO_TSELECT, i);
	register_write_direct(target, GDB_REGNO_TDATA1, 0);
	info->trigger_unique_id[i] = -1;

	return ERROR_OK;
}

static void trigger_from_breakpoint(struct trigger *trigger,
		const struct breakpoint *breakpoint)
{
	trigger->address = breakpoint->address;
	trigger->length = breakpoint->length;
	trigger->mask = ~0LL;
	trigger->read = false;
	trigger->write = false;
	trigger->execute = true;
	// unique_id is unique across both breakpoints and watchpoints.
	trigger->unique_id = breakpoint->unique_id;
}

static void trigger_from_watchpoint(struct trigger *trigger,
		const struct watchpoint *watchpoint)
{
	trigger->address = watchpoint->address;
	trigger->length = watchpoint->length;
	trigger->mask = watchpoint->mask;
	trigger->value = watchpoint->value;
	trigger->read = (watchpoint->rw == WPT_READ || watchpoint->rw == WPT_ACCESS);
	trigger->write = (watchpoint->rw == WPT_WRITE || watchpoint->rw == WPT_ACCESS);
	trigger->execute = false;
	// unique_id is unique across both breakpoints and watchpoints.
	trigger->unique_id = watchpoint->unique_id;
}

static int add_breakpoint(struct target *target,
	struct breakpoint *breakpoint)
{
	if (breakpoint->type == BKPT_SOFT) {
		if (target_read_memory(target, breakpoint->address, breakpoint->length, 1,
					breakpoint->orig_instr) != ERROR_OK) {
			LOG_ERROR("Failed to read original instruction at 0x%x",
					breakpoint->address);
			return ERROR_FAIL;
		}

		int retval;
		if (breakpoint->length == 4) {
			retval = target_write_u32(target, breakpoint->address, ebreak());
		} else {
			retval = target_write_u16(target, breakpoint->address, ebreak_c());
		}
		if (retval != ERROR_OK) {
			LOG_ERROR("Failed to write %d-byte breakpoint instruction at 0x%x",
					breakpoint->length, breakpoint->address);
			return ERROR_FAIL;
		}

	} else if (breakpoint->type == BKPT_HARD) {
		struct trigger trigger;
		trigger_from_breakpoint(&trigger, breakpoint);
		int result = add_trigger(target, &trigger);
		if (result != ERROR_OK) {
			return result;
		}
	} else {
		LOG_INFO("OpenOCD only supports hardware and software breakpoints.");
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}

	breakpoint->set = true;

	return ERROR_OK;
}

static int remove_breakpoint(struct target *target,
		struct breakpoint *breakpoint)
{
	if (breakpoint->type == BKPT_SOFT) {
		if (target_write_memory(target, breakpoint->address, breakpoint->length, 1,
					breakpoint->orig_instr) != ERROR_OK) {
			LOG_ERROR("Failed to restore instruction for %d-byte breakpoint at "
					"0x%x", breakpoint->length, breakpoint->address);
			return ERROR_FAIL;
		}

	} else if (breakpoint->type == BKPT_HARD) {
		struct trigger trigger;
		trigger_from_breakpoint(&trigger, breakpoint);
		int result = remove_trigger(target, &trigger);
		if (result != ERROR_OK) {
			return result;
		}
	} else {
		LOG_INFO("OpenOCD only supports hardware and software breakpoints.");
		return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
	}

	breakpoint->set = false;

	return ERROR_OK;
}

static int add_watchpoint(struct target *target,
		struct watchpoint *watchpoint)
{
	struct trigger trigger;
	trigger_from_watchpoint(&trigger, watchpoint);

	int result = add_trigger(target, &trigger);
	if (result != ERROR_OK) {
		return result;
	}
	watchpoint->set = true;

	return ERROR_OK;
}

static int remove_watchpoint(struct target *target,
		struct watchpoint *watchpoint)
{
	struct trigger trigger;
	trigger_from_watchpoint(&trigger, watchpoint);

	int result = remove_trigger(target, &trigger);
	if (result != ERROR_OK) {
		return result;
	}
	watchpoint->set = false;

	return ERROR_OK;
}

static int examine(struct target *target)
{
	// Don't need to select dbus, since the first thing we do is read dtmcontrol.

	uint32_t dtmcontrol = dtmcontrol_scan(target, 0);
	LOG_DEBUG("dtmcontrol=0x%x", dtmcontrol);
	LOG_DEBUG("  dmireset=%d", get_field(dtmcontrol, DTM_DTMCS_DMIRESET));
	LOG_DEBUG("  idle=%d", get_field(dtmcontrol, DTM_DTMCS_IDLE));
	LOG_DEBUG("  dmistat=%d", get_field(dtmcontrol, DTM_DTMCS_DMISTAT));
	LOG_DEBUG("  abits=%d", get_field(dtmcontrol, DTM_DTMCS_ABITS));
	LOG_DEBUG("  version=%d", get_field(dtmcontrol, DTM_DTMCS_VERSION));
	if (dtmcontrol == 0) {
		LOG_ERROR("dtmcontrol is 0. Check JTAG connectivity/board power.");
		return ERROR_FAIL;
	}
	if (get_field(dtmcontrol, DTM_DTMCS_VERSION) != 1) {
		LOG_ERROR("Unsupported DTM version %d. (dtmcontrol=0x%x)",
				get_field(dtmcontrol, DTM_DTMCS_VERSION), dtmcontrol);
		return ERROR_FAIL;
	}

	riscv013_info_t *info = get_info(target);
	info->abits = get_field(dtmcontrol, DTM_DTMCS_ABITS);
	info->dtmcontrol_idle = get_field(dtmcontrol, DTM_DTMCS_IDLE);

	uint32_t dmcontrol = dmi_read(target, DMI_DMCONTROL);
	uint32_t dmstatus = dmi_read(target, DMI_DMSTATUS);
	if (get_field(dmstatus, DMI_DMSTATUS_VERSIONLO) != 2) {
		LOG_ERROR("OpenOCD only supports Debug Module version 2, not %d "
				"(dmstatus=0x%x)", get_field(dmstatus, DMI_DMSTATUS_VERSIONLO), dmstatus);
		return ERROR_FAIL;
	}

	// Reset the Debug Module.
	dmi_write(target, DMI_DMCONTROL, 0);
	dmi_write(target, DMI_DMCONTROL, DMI_DMCONTROL_DMACTIVE);
	dmcontrol = dmi_read(target, DMI_DMCONTROL);

	LOG_DEBUG("dmcontrol: 0x%08x", dmcontrol);
	LOG_DEBUG("dmstatus:  0x%08x", dmstatus);

	if (!get_field(dmcontrol, DMI_DMCONTROL_DMACTIVE)) {
		LOG_ERROR("Debug Module did not become active. dmcontrol=0x%x",
				dmcontrol);
		return ERROR_FAIL;
	}

	if (!get_field(dmstatus, DMI_DMSTATUS_AUTHENTICATED)) {
		LOG_ERROR("Authentication required by RISC-V core but not "
				"supported by OpenOCD. dmcontrol=0x%x", dmcontrol);
		return ERROR_FAIL;
	}

	if (get_field(dmstatus, DMI_DMSTATUS_ANYUNAVAIL)) {
		LOG_ERROR("The hart is unavailable.");
		return ERROR_FAIL;
	}

	if (get_field(dmstatus, DMI_DMSTATUS_ANYNONEXISTENT)) {
		LOG_ERROR("The hart doesn't exist.");
		return ERROR_FAIL;
	}

	// Check that abstract data registers are accessible.
	uint32_t abstractcs = dmi_read(target, DMI_ABSTRACTCS);
	info->datacount = get_field(abstractcs, DMI_ABSTRACTCS_DATACOUNT);
	info->progsize = get_field(abstractcs, DMI_ABSTRACTCS_PROGSIZE);

	/* Before doing anything else we must first enumerate the harts. */
	RISCV_INFO(r);
	for (int i = 0; i < RISCV_MAX_HARTS; ++i) {
		riscv_set_current_hartid(target, i);
		uint32_t s = dmi_read(target, DMI_DMSTATUS);
		if (get_field(s, DMI_DMSTATUS_ANYNONEXISTENT))
			break;
		r->hart_count = i + 1;
	}

	/* FIXME: This is broken. */
	LOG_ERROR("Enumerated %d harts, but there's an off-by-one error in the hardware", r->hart_count);
	r->hart_count--;

	/* Halt every hart so we can probe them. */
	riscv_halt_all_harts(target);

	/* Examines every hart, first checking XLEN. */
	for (int i = 0; i < riscv_count_harts(target); ++i) {
		RISCV_INFO(r);
		riscv_set_current_hartid(target, i);

		if (abstract_read_register(target, NULL, S0, 128) == ERROR_OK) {
			r->xlen[i] = 128;
		} else if (abstract_read_register(target, NULL, S0, 64) == ERROR_OK) {
			r->xlen[i] = 64;
		} else if (abstract_read_register(target, NULL, S0, 32) == ERROR_OK) {
			r->xlen[i] = 32;
		} else {
			LOG_ERROR("Failed to discover size using abstract register reads.");
			return ERROR_FAIL;
		}
	}

	/* FIXME: Are there 2 triggers? */
	info->trigger_count = 2;

	/* Resumes all the harts, so the debugger can later pause them. */
	riscv_resume_all_harts(target);
	target_set_examined(target);
	return ERROR_OK;
}

static int assert_reset(struct target *target)
{
	return ERROR_FAIL;
}

static int deassert_reset(struct target *target)
{
	return ERROR_FAIL;
}

/**
 * If there was a DMI error, clear that error and return 1.
 * Otherwise return 0.
 */
static int check_dmi_error(struct target *target)
{
	dmi_status_t status = dmi_scan(target, NULL, NULL, DMI_OP_NOP, 0, 0,
			false);
	if (status != DMI_STATUS_SUCCESS) {
		// Clear errors.
		dtmcontrol_scan(target, DTM_DTMCS_DMIRESET);
		increase_dmi_busy_delay(target);
		return 1;
	}
	return 0;
}

static int read_memory(struct target *target, uint32_t address,
		uint32_t size, uint32_t count, uint8_t *buffer)
{
	select_dmi(target);
	riscv_set_current_hartid(target, 0);

	for (uint32_t i = 0; i < count; ++i) {
		uint32_t offset = i*size;
		uint32_t t_addr = address + offset;
		uint8_t *t_buffer = buffer + offset;

		abstract_write_register(target, S0, riscv_xlen(target), t_addr);

		program_t *program = program_new();
		switch (size) {
		case 1:
			program_add32(program, lb(S1, S0, 0));
			break;
		case 2:
			program_add32(program, lh(S1, S0, 0));
			break;
		case 4:
			program_add32(program, lw(S1, S0, 0));
			break;
		default:
			LOG_ERROR("Unsupported size: %d", size);
			return ERROR_FAIL;
		}
		program_add32(program, fence());
		program_add32(program, ebreak());
		program_set_read(program, S1);
		write_program(target, program);
		execute_program(target, program);
		uint32_t abstractcs;
		wait_for_idle(target, &abstractcs);
		program_delete(program);

		uint32_t value = dmi_read(target, DMI_DATA0);
		switch (size) {
		case 1:
			t_buffer[0] = value;
			break;
		case 2:
		  t_buffer[0] = value;
		  t_buffer[1] = value >> 8;
		  break;
		case 4:
		  t_buffer[0] = value;
		  t_buffer[1] = value >> 8;
		  t_buffer[2] = value >> 16;
		  t_buffer[3] = value >> 24;
		  break;
		default:
		  return ERROR_FAIL;
		}

		LOG_INFO("read 0x%08x from 0x%08x", value, t_addr);

		if (check_dmi_error(target)) {
			LOG_ERROR("DMI error");
			return ERROR_FAIL;
		}
	}

	program_t *program = program_new();
	program_add32(program, ebreak());
	program_add32(program, ebreak());
	program_add32(program, ebreak());
	program_add32(program, ebreak());
	write_program(target, program);
	program_delete(program);

	return ERROR_OK;
}

static int write_memory(struct target *target, uint32_t address,
		uint32_t size, uint32_t count, const uint8_t *buffer)
{
	select_dmi(target);
	riscv_set_current_hartid(target, 0);

	for (uint32_t i = 0; i < count; ++i) {
		uint32_t offset = size*i;
		uint32_t t_addr = address + offset;
		const uint8_t *t_buffer = buffer + offset;

		abstract_write_register(target, S0, riscv_xlen(target), t_addr);
		program_t *program = program_new();
		switch (size) {
			case 1:
				program_add32(program, sb(S1, S0, 0));
				break;
			case 2:
				program_add32(program, sh(S1, S0, 0));
				break;
			case 4:
				program_add32(program, sw(S1, S0, 0));
				break;
			default:
				LOG_ERROR("Unsupported size: %d", size);
				return ERROR_FAIL;
		}
		program_add32(program, fence());
		program_add32(program, ebreak());

		uint32_t value;
		switch (size) {
			case 1:
				value = t_buffer[0];
				break;
			case 2:
				value = t_buffer[0] | ((uint32_t) t_buffer[1] << 8);
				break;
			case 4:
				value = t_buffer[0] |
					((uint32_t) t_buffer[1] << 8) |
					((uint32_t) t_buffer[2] << 16) |
					((uint32_t) t_buffer[3] << 24);
				break;
			default:
				return ERROR_FAIL;
		}
		abstract_write_register(target, S1, riscv_xlen(target), value);
		program_set_write(program, S1, value);

		LOG_INFO("writing 0x%08x to 0x%08x", value, t_addr);

		write_program(target, program);
		execute_program(target, program);
		uint32_t abstractcs;
		wait_for_idle(target, &abstractcs);
		program_delete(program);

		if (check_dmi_error(target)) {
			LOG_ERROR("DMI error");
			return ERROR_FAIL;
		}
	}

	program_t *program = program_new();
	program_add32(program, ebreak());
	program_add32(program, ebreak());
	program_add32(program, ebreak());
	program_add32(program, ebreak());
	write_program(target, program);
	program_delete(program);
	return ERROR_OK;
}

static int arch_state(struct target *target)
{
	return ERROR_OK;
}

struct target_type riscv013_target =
{
	.name = "riscv",

	.init_target = init_target,
	.deinit_target = deinit_target,
	.examine = examine,

	.poll = &riscv_openocd_poll,
	.halt = &riscv_openocd_halt,
	.resume = &riscv_openocd_resume,
	.step = &riscv_openocd_step,

	.assert_reset = assert_reset,
	.deassert_reset = deassert_reset,

	.read_memory = read_memory,
	.write_memory = write_memory,

	.add_breakpoint = add_breakpoint,
	.remove_breakpoint = remove_breakpoint,

	.add_watchpoint = add_watchpoint,
	.remove_watchpoint = remove_watchpoint,

	.arch_state = arch_state,
};

/*** 0.13-specific implementations of various RISC-V hepler functions. ***/
static riscv_reg_t riscv013_get_register(struct target *target, int hid, int rid)
{
	riscv_set_current_hartid(target, hid);

	uint64_t out;
	register_read_direct(target, &out, rid);
	return out;
}

static void riscv013_set_register(struct target *target, int hid, int rid, uint64_t value)
{
	riscv_set_current_hartid(target, hid);

	register_write_direct(target, rid, value);
}

static void riscv013_select_current_hart(struct target *target)
{
	RISCV_INFO(r);

	uint64_t dmcontrol = dmi_read(target, DMI_DMCONTROL);
	dmcontrol = set_field(dmcontrol, DMI_DMCONTROL_HARTSEL, r->current_hartid);
	dmi_write(target, DMI_DMCONTROL, dmcontrol);
}

static void riscv013_halt_current_hart(struct target *target)
{
	RISCV_INFO(r);
	LOG_DEBUG("halting hart %d", r->current_hartid);
	assert(!riscv_is_halted(target));

	/* Issue the halt command, and then wait for the current hart to halt. */
	uint32_t dmcontrol = dmi_read(target, DMI_DMCONTROL);
	dmcontrol = set_field(dmcontrol, DMI_DMCONTROL_HALTREQ, 1);
	dmi_write(target, DMI_DMCONTROL, dmcontrol);
	for (size_t i = 0; i < 256; ++i)
		if (riscv_is_halted(target))
			break;

	if (!riscv_is_halted(target)) {
		uint32_t dmstatus = dmi_read(target, DMI_DMSTATUS);
		dmcontrol = dmi_read(target, DMI_DMCONTROL);

		LOG_ERROR("unable to halt hart %d", r->current_hartid);
		LOG_ERROR("  dmcontrol=0x%08x", dmcontrol);
		LOG_ERROR("  dmstatus =0x%08x", dmstatus);
		abort();
	}

	dmcontrol = set_field(dmcontrol, DMI_DMCONTROL_HALTREQ, 0);
	dmi_write(target, DMI_DMCONTROL, dmcontrol);
}

static void riscv013_resume_current_hart(struct target *target)
{
	return riscv013_step_or_resume_current_hart(target, false);
}

static void riscv013_step_current_hart(struct target *target)
{
	return riscv013_step_or_resume_current_hart(target, true);
}

static void riscv013_on_resume(struct target *target)
{
	return riscv013_on_step_or_resume(target, false);
}

static void riscv013_on_step(struct target *target)
{
	return riscv013_on_step_or_resume(target, true);
}

static void riscv013_on_halt(struct target *target)
{
	RISCV_INFO(r);
	LOG_DEBUG("saving register state for hart %d", r->current_hartid);
	riscv_save_register(target, GDB_REGNO_S0);
	riscv_save_register(target, GDB_REGNO_S1);
	riscv_save_register(target, GDB_REGNO_DPC);
	riscv_save_register(target, GDB_REGNO_DCSR);
}

static bool riscv013_is_halted(struct target *target)
{
	uint32_t dmstatus = dmi_read(target, DMI_DMSTATUS);
	return get_field(dmstatus, DMI_DMSTATUS_ALLHALTED);
}

static enum riscv_halt_reason riscv013_halt_reason(struct target *target)
{
	uint64_t dcsr = riscv_peek_register(target, GDB_REGNO_DCSR);
	switch (get_field(dcsr, CSR_DCSR_CAUSE)) {
	case CSR_DCSR_CAUSE_SWBP:
	case CSR_DCSR_CAUSE_TRIGGER:
		return RISCV_HALT_BREAKPOINT;
	case CSR_DCSR_CAUSE_STEP:
		return RISCV_HALT_SINGLESTEP;
	case CSR_DCSR_CAUSE_DEBUGINT:
	case CSR_DCSR_CAUSE_HALT:
		return RISCV_HALT_INTERRUPT;
	}

	LOG_ERROR("Unknown DCSR cause field: %x", (int)get_field(dcsr, CSR_DCSR_CAUSE));
	abort();
}

/* Helper Functions. */
static void riscv013_on_step_or_resume(struct target *target, bool step)
{
	RISCV_INFO(r);
	LOG_DEBUG("restoring register state for hart %d", r->current_hartid);

	program_t *program = program_new();
	program_add32(program, fence_i());
	program_add32(program, ebreak());
	write_program(target, program);
	if (execute_program(target, program) != ERROR_OK) {
		LOG_ERROR("Unable to execute fence.i");
	}
	program_delete(program);

	/* We want to twiddle some bits in the debug CSR so debugging works. */
	uint64_t dcsr = riscv_peek_register(target, GDB_REGNO_DCSR);
	dcsr = set_field(dcsr, CSR_DCSR_STEP, step);
	dcsr = set_field(dcsr, CSR_DCSR_EBREAKM, 1);
	dcsr = set_field(dcsr, CSR_DCSR_EBREAKH, 1);
	dcsr = set_field(dcsr, CSR_DCSR_EBREAKS, 1);
	dcsr = set_field(dcsr, CSR_DCSR_EBREAKU, 1);
	riscv_overwrite_register(target, GDB_REGNO_DCSR, dcsr);

	riscv_restore_register(target, GDB_REGNO_DCSR);
	riscv_restore_register(target, GDB_REGNO_DPC);
	riscv_restore_register(target, GDB_REGNO_S1);
	riscv_restore_register(target, GDB_REGNO_S0);
}

static void riscv013_step_or_resume_current_hart(struct target *target, bool step)
{
	RISCV_INFO(r);
	LOG_DEBUG("resuming hart %d", r->current_hartid);
	assert(riscv_is_halted(target));

	/* Issue the halt command, and then wait for the current hart to halt. */
	uint32_t dmcontrol = dmi_read(target, DMI_DMCONTROL);
	dmcontrol = set_field(dmcontrol, DMI_DMCONTROL_RESUMEREQ, 1);
	dmi_write(target, DMI_DMCONTROL, dmcontrol);

#if 1
	/* FIXME: ... well, after a short time. */
	usleep(100);
#else
	/* FIXME: there's a race condition in stepping now, so just return
	 * right away... */
	for (size_t i = 0; i < 256; ++i) {
		if (!riscv_is_halted(target))
			break;
	}

	if (riscv_is_halted(target)) {
		uint32_t dmstatus = dmi_read(target, DMI_DMSTATUS);
		dmcontrol = dmi_read(target, DMI_DMCONTROL);

		LOG_ERROR("unable to resume hart %d", r->current_hartid);
		LOG_ERROR("  dmcontrol=0x%08x", dmcontrol);
		LOG_ERROR("  dmstatus =0x%08x", dmstatus);
		abort();
	}
#endif

	dmcontrol = set_field(dmcontrol, DMI_DMCONTROL_RESUMEREQ, 0);
	dmi_write(target, DMI_DMCONTROL, dmcontrol);
}
