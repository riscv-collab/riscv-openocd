#include <stdbool.h>
#include <string.h>
#include "target/breakpoints.h"
#include "target/target.h"
#include "riscv.h"
#include "encoding.h"

bool riscv_syntacore_has_csr(unsigned csr_number,
                              struct target *target) {
	(void)target;
	switch(csr_number) {
	case CSR_CYCLE:
	case CSR_INSTRET:
	case CSR_MARCHID:
	case CSR_MCAUSE:
	case CSR_MCOUNTEREN:
	case CSR_MCYCLE:
	case CSR_MEPC:
	case CSR_MHARTID:
	case CSR_MIE:
	case CSR_MIMPID:
	case CSR_MINSTRET:
	case CSR_MIP:
	case CSR_MISA:
	case CSR_MSCRATCH:
	case CSR_MSTATUS:
	case CSR_MTVAL:
	case CSR_MTVEC:
	case CSR_MVENDORID:
	case CSR_TIME:
	/* These are required for watchpoint functionality
	case CSR_TSELECT:
	case CSR_TDATA1:
	*/
		return true;
	default:
		return false;
	}
}

bool riscv_syntacore_csr_force_default_disabled(unsigned csr_number) {
	switch(csr_number) {
	case CSR_HPMCOUNTER3H:
	case CSR_HPMCOUNTER4H:
	case CSR_HPMCOUNTER5H:
	case CSR_HPMCOUNTER6H:
	case CSR_HPMCOUNTER7H:
	case CSR_HPMCOUNTER8H:
	case CSR_HPMCOUNTER9H:
	case CSR_HPMCOUNTER10H:
	case CSR_HPMCOUNTER11H:
	case CSR_HPMCOUNTER12H:
	case CSR_HPMCOUNTER13H:
	case CSR_HPMCOUNTER14H:
	case CSR_HPMCOUNTER15H:
	case CSR_HPMCOUNTER16H:
	case CSR_HPMCOUNTER17H:
	case CSR_HPMCOUNTER18H:
	case CSR_HPMCOUNTER19H:
	case CSR_HPMCOUNTER20H:
	case CSR_HPMCOUNTER21H:
	case CSR_HPMCOUNTER22H:
	case CSR_HPMCOUNTER23H:
	case CSR_HPMCOUNTER24H:
	case CSR_HPMCOUNTER25H:
	case CSR_HPMCOUNTER26H:
	case CSR_HPMCOUNTER27H:
	case CSR_HPMCOUNTER28H:
	case CSR_HPMCOUNTER29H:
	case CSR_HPMCOUNTER30H:
	case CSR_HPMCOUNTER31H:
		return true;
	case CSR_MHPMCOUNTER3H:
	case CSR_MHPMCOUNTER4H:
	case CSR_MHPMCOUNTER5H:
	case CSR_MHPMCOUNTER6H:
	case CSR_MHPMCOUNTER7H:
	case CSR_MHPMCOUNTER8H:
	case CSR_MHPMCOUNTER9H:
	case CSR_MHPMCOUNTER10H:
	case CSR_MHPMCOUNTER11H:
	case CSR_MHPMCOUNTER12H:
	case CSR_MHPMCOUNTER13H:
	case CSR_MHPMCOUNTER14H:
	case CSR_MHPMCOUNTER15H:
	case CSR_MHPMCOUNTER16H:
	case CSR_MHPMCOUNTER17H:
	case CSR_MHPMCOUNTER18H:
	case CSR_MHPMCOUNTER19H:
	case CSR_MHPMCOUNTER20H:
	case CSR_MHPMCOUNTER21H:
	case CSR_MHPMCOUNTER22H:
	case CSR_MHPMCOUNTER23H:
	case CSR_MHPMCOUNTER24H:
	case CSR_MHPMCOUNTER25H:
	case CSR_MHPMCOUNTER26H:
	case CSR_MHPMCOUNTER27H:
	case CSR_MHPMCOUNTER28H:
	case CSR_MHPMCOUNTER29H:
	case CSR_MHPMCOUNTER30H:
	case CSR_MHPMCOUNTER31H:
		return true;
	default:
		return false;
	}
}

static const char* const default_prefix = "csr";

bool riscv_syntacore_csr_expose_and_rename(struct reg *r,
				unsigned csr_number, const range_list_t *entry) {
	const size_t prefix_len = strlen(default_prefix);
	// we can rename only registers that have default names like csrXXX,
	// where XXX is a decimal number
	if (!(r->name && entry->name &&
			strncmp(entry->name, default_prefix, prefix_len) == 0))
		return false;

	char *parse_end = 0;
	long passed_num = strtoul(entry->name + prefix_len, &parse_end, 10);
	// if parse_end does not point to '\0' - something went wrong'
	if (!*parse_end)
		return false;
	if ((passed_num <= 0) || (passed_num != csr_number))
		return false;
	// make sure that r->name is not an empty string
	if (!*(r->name))
		return false;

	r->exist = true;
	LOG_DEBUG("Exposing spec-compliant CSR %d (name=%s)", csr_number, r->name);
	return true;
}

const char *
riscv_syntacore_get_legacy_gpr_name_by_gdb_regno(enum gdb_regno number) {
	switch (number) {
		case GDB_REGNO_ZERO:
			return "x0";
		case GDB_REGNO_RA:
			return "x1";
		case GDB_REGNO_SP:
			return "x2";
		case GDB_REGNO_GP:
			return "x3";
		case GDB_REGNO_TP:
			return "x4";
		case GDB_REGNO_T0:
			return "x5";
		case GDB_REGNO_T1:
			return "x6";
		case GDB_REGNO_T2:
			return "x7";
		case GDB_REGNO_FP:
			return "x8";
		case GDB_REGNO_S1:
			return "x9";
		case GDB_REGNO_A0:
			return "x10";
		case GDB_REGNO_A1:
			return "x11";
		case GDB_REGNO_A2:
			return "x12";
		case GDB_REGNO_A3:
			return "x13";
		case GDB_REGNO_A4:
			return "x14";
		case GDB_REGNO_A5:
			return "x15";
		case GDB_REGNO_A6:
			return "x16";
		case GDB_REGNO_A7:
			return "x17";
		case GDB_REGNO_S2:
			return "x18";
		case GDB_REGNO_S3:
			return "x19";
		case GDB_REGNO_S4:
			return "x20";
		case GDB_REGNO_S5:
			return "x21";
		case GDB_REGNO_S6:
			return "x22";
		case GDB_REGNO_S7:
			return "x23";
		case GDB_REGNO_S8:
			return "x24";
		case GDB_REGNO_S9:
			return "x25";
		case GDB_REGNO_S10:
			return "x26";
		case GDB_REGNO_S11:
			return "x27";
		case GDB_REGNO_T3:
			return "x28";
		case GDB_REGNO_T4:
			return "x29";
		case GDB_REGNO_T5:
			return "x30";
		case GDB_REGNO_T6:
			return "x31";
		default:
			return NULL;
	}
}

const char *
riscv_syntacore_get_legacy_fpr_name_by_gdb_regno(enum gdb_regno number) {
	switch (number) {
		case GDB_REGNO_FT0:
			return "f0";
		case GDB_REGNO_FT1:
			return "f1";
		case GDB_REGNO_FT2:
			return "f2";
		case GDB_REGNO_FT3:
			return "f3";
		case GDB_REGNO_FT4:
			return "f4";
		case GDB_REGNO_FT5:
			return "f5";
		case GDB_REGNO_FT6:
			return "f6";
		case GDB_REGNO_FT7:
			return "f7";
		case GDB_REGNO_FS0:
			return "f8";
		case GDB_REGNO_FS1:
			return "f9";
		case GDB_REGNO_FA0:
			return "f10";
		case GDB_REGNO_FA1:
			return "f11";
		case GDB_REGNO_FA2:
			return "f12";
		case GDB_REGNO_FA3:
			return "f13";
		case GDB_REGNO_FA4:
			return "f14";
		case GDB_REGNO_FA5:
			return "f15";
		case GDB_REGNO_FA6:
			return "f16";
		case GDB_REGNO_FA7:
			return "f17";
		case GDB_REGNO_FS2:
			return "f18";
		case GDB_REGNO_FS3:
			return "f19";
		case GDB_REGNO_FS4:
			return "f20";
		case GDB_REGNO_FS5:
			return "f21";
		case GDB_REGNO_FS6:
			return "f22";
		case GDB_REGNO_FS7:
			return "f23";
		case GDB_REGNO_FS8:
			return "f24";
		case GDB_REGNO_FS9:
			return "f25";
		case GDB_REGNO_FS10:
			return "f26";
		case GDB_REGNO_FS11:
			return "f27";
		case GDB_REGNO_FT8:
			return "f28";
		case GDB_REGNO_FT9:
			return "f29";
		case GDB_REGNO_FT10:
			return "f30";
		case GDB_REGNO_FT11:
			return "f31";
		default:
			return NULL;
	}
}

COMMAND_HANDLER(riscv_syntacore_use_abi_regnames)
{
	if (CMD_ARGC) {
		LOG_ERROR("Command takes no arguments");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	struct target *target = get_current_target(CMD_CTX);
	RISCV_INFO(r);
	r->use_abi_regnames = true;
	return ERROR_OK;
}

const struct command_registration riscv_syntacore_command_handlers[] = {
	{
		.name = "use_abi_regnames",
		.handler = riscv_syntacore_use_abi_regnames,
		.mode = COMMAND_CONFIG,
		.usage = "",
		.help = "If passed, abi names are used for register names"
	},
	COMMAND_REGISTRATION_DONE
};
