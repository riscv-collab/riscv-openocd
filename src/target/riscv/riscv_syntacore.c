#include <stdbool.h>
#include "target/breakpoints.h"
#include "target/target.h"
#include "riscv.h"
#include "encoding.h"

bool riscv_syntactore_has_csr(unsigned csr_number,
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
