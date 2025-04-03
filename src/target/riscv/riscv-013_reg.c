// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "riscv-013_reg.h"
#include "field_helpers.h"

#include "riscv_reg.h"
#include "riscv_reg_impl.h"
#include "riscv-013.h"
#include "debug_defines.h"
#include <helper/time_support.h>

/* Start of register fetch/send definitions.
 * - "reg_fetcher" (named "fetch_*") -- loads the value from target into reg
 *   cache entry. Modifies only "reg->value".
 * - "reg_sender" (named "send_*") -- stores the value from "buf" to the
 *   target. Does not modify "reg" at all.
 */
typedef int (*reg_fetcher)(struct reg *reg);
typedef int (*reg_sender)(const struct reg *reg, const uint8_t *buf);

/* TODO: Introduce separate "fetch_*" and "send_*" functions for register
 * classes (xregs, fregs, csrs).
 */
static int fetch_riscv013_reg(struct reg *reg)
{
	riscv_reg_t value;
	int res = riscv013_get_register(riscv_reg_impl_get_target(reg),
			&value, reg->number);
	if (res != ERROR_OK)
		return res;
	buf_set_u64(reg->value, 0, reg->size, value);
	return res;
}

static int send_riscv013_reg(const struct reg *reg, const uint8_t *buf)
{
	assert(reg->size <= 64);
	const riscv_reg_t value = buf_get_u64(buf, 0, reg->size);
	return riscv013_set_register(riscv_reg_impl_get_target(reg),
			reg->number, value);
}

/* TODO: Inline "riscv013_*_register_buf" into "vreg" accessors.
 */
static int fetch_vreg(struct reg *reg)
{
	return riscv013_get_register_buf(riscv_reg_impl_get_target(reg),
			reg->value, reg->number);
}

static int send_vreg(const struct reg *reg, const uint8_t *buf)
{
	return riscv013_set_register_buf(riscv_reg_impl_get_target(reg),
			reg->number, reg->value);
}

/* End of register fetch/send definitions. */
/* Start of cache entry utils. */

static void set_cache_value(struct reg *reg, const uint8_t *buf)
{
	assert(riscv_reg_impl_is_existing(reg));
	buf_cpy(buf, reg->value, reg->size);
}

/**
 * Note: There are only 3 reachable states for an item in the register cache:
 * |: state   :|: "reg->valid" :|: "reg->dirty" :|
 * | "invalid" | false          | false          |
 * | "valid"   | true           | false          |
 * | "dirty"   | true           | true           |
 *
 * The state "reg->valid == dirty && reg->valid == false" should be unreacheable.
 * To achieve this, "reg->valid" and "reg->dirty" are not assigned directly
 * outside of "mark_<state>()" functions.
 */
static void mark_invalid(struct reg *reg)
{
	assert(riscv_reg_impl_is_existing(reg));
	reg->valid = false;
	reg->dirty = false;
}

static void mark_clean(struct reg *reg)
{
	assert(riscv_reg_impl_is_existing(reg));
	assert(riscv_reg_impl_get_target(reg)->state == TARGET_HALTED);
	reg->valid = true;
	reg->dirty = false;
}

static void mark_dirty(struct reg *reg)
{
	assert(riscv_reg_impl_is_existing(reg));
	assert(riscv_reg_impl_get_target(reg)->state == TARGET_HALTED);
	reg->valid = true;
	reg->dirty = true;
}

static int access_prologue(const struct reg *reg, bool write)
{
	assert(riscv_reg_impl_is_initialized(reg));
	const struct target * const target = riscv_reg_impl_get_target(reg);
	if (!reg->exist) {
		LOG_TARGET_DEBUG(target, "Register %s does not exist",
				reg->name);
		return ERROR_FAIL;
	}
	LOG_TARGET_DEBUG(target, "%s %s (valid=%s, dirty=%s)",
			write ? "Writing" : "Reading",
			reg->name,
			reg->valid ? "true" : "false",
			reg->dirty ? "true" : "false");
	return ERROR_OK;
}

static void access_epilogue(const struct reg *reg, bool write)
{
	assert(riscv_reg_impl_is_existing(reg));
	if (debug_level < LOG_LVL_DEBUG)
		return;
	char *value_hex = buf_to_hex_str(reg->value, reg->size);
	LOG_TARGET_DEBUG(riscv_reg_impl_get_target(reg),
			"%s 0x%s %s %s (valid=%s, dirty=%s)",
			write ? "Wrote" : "Read",
			value_hex,
			write ? "to" : "from",
			reg->name,
			reg->valid ? "true" : "false",
			reg->dirty ? "true" : "false");
	free(value_hex);
}

static int noncaching_get(struct reg *reg, reg_fetcher fetch_reg)
{
	assert(riscv_reg_impl_is_initialized(reg));
	int res = access_prologue(reg, /*write*/ false);
	if (res != ERROR_OK)
		return res;
	res = fetch_reg(reg);
	mark_invalid(reg);
	if (res != ERROR_OK)
		return res;
	access_epilogue(reg, /*write*/ false);
	return ERROR_OK;
}

static int caching_get(struct reg *reg, reg_fetcher fetch_reg)
{
	assert(riscv_reg_impl_is_initialized(reg));
	int res = access_prologue(reg, /*write*/ false);
	if (res != ERROR_OK)
		return res;
	if (reg->valid) {
		LOG_TARGET_DEBUG(riscv_reg_impl_get_target(reg),
				"Reading %s from cache", reg->name);
		return ERROR_OK;
	}
	res = fetch_reg(reg);
	if (res != ERROR_OK) {
		mark_invalid(reg);
		return res;
	}
	if (riscv_reg_impl_get_target(reg)->state == TARGET_HALTED)
		mark_clean(reg);
	else
		mark_invalid(reg);
	access_epilogue(reg, /*write*/ false);
	return ERROR_OK;
}

static int noncaching_set(struct reg *reg, const uint8_t *buf,
		reg_sender send_reg)
{
	int res = access_prologue(reg, /*write*/ true);
	if (res != ERROR_OK)
		return res;
	res = send_reg(reg, buf);
	mark_invalid(reg);
	if (res != ERROR_OK)
		return res;
	set_cache_value(reg, buf);
	access_epilogue(reg, /*write*/ true);
	return ERROR_OK;
}

static int caching_set(struct reg *reg, const uint8_t *buf,
		reg_sender send_reg)
{
	assert(riscv_reg_impl_is_initialized(reg));
	const struct target * const target = riscv_reg_impl_get_target(reg);
	if (target->state != TARGET_HALTED) {
		LOG_TARGET_DEBUG(target, "Not caching the write to %s", reg->name);
		return noncaching_set(reg, buf, send_reg);
	}
	int res = access_prologue(reg, /*write*/ true);
	if (res != ERROR_OK)
		return res;
	if (reg->valid && buf_eq(reg->value, buf, reg->size)) {
		LOG_TARGET_DEBUG(riscv_reg_impl_get_target(reg),
				"Writing the same value to %s", reg->name);
	} else {
		LOG_TARGET_DEBUG(target, "Caching the write to %s", reg->name);
		set_cache_value(reg, buf);
		mark_dirty(reg);
	}
	access_epilogue(reg, /*write*/ true);
	return ERROR_OK;
}

static int flush_reg(struct reg *reg, reg_sender send_reg)
{
	if (!reg->dirty)
		return ERROR_OK;
	int res = access_prologue(reg, /*write*/ true);
	if (res != ERROR_OK)
		return res;
	assert(reg->dirty);
	assert(reg->valid);
	const struct target * const target = riscv_reg_impl_get_target(reg);
	if (target->state != TARGET_HALTED) {
		LOG_TARGET_ERROR(target,
				"BUG: register %s is dirty while the target is not halted",
				reg->name);
		return ERROR_TARGET_NOT_HALTED;
	}
	res = send_reg(reg, reg->value);
	if (res != ERROR_OK) {
		/* Register is not marked "invalid" here (like it's done on failure in
		 * "caching_set") since it is "dirty", i.e. the most recent value
		 * is currently in the cache. */
		return res;
	}
	mark_clean(reg);
	access_epilogue(reg, /*write*/ true);
	return res;
}

/* End of cache entry utils. */
/* Start of "reg_arch_type" method definitions. */

static int riscv013_noncaching_get(struct reg *reg)
{
	return noncaching_get(reg, fetch_riscv013_reg);
}

static int riscv013_caching_get(struct reg *reg)
{
	return caching_get(reg, fetch_riscv013_reg);
}

static int riscv013_noncaching_set(struct reg *reg, uint8_t *buf)
{
	return noncaching_set(reg, buf, send_riscv013_reg);
}

static int riscv013_caching_set(struct reg *reg, uint8_t *buf)
{
	return caching_set(reg, buf, send_riscv013_reg);
}

static int riscv013_reg_flush(struct reg *reg)
{
	return flush_reg(reg, send_riscv013_reg);
}

static int flush_noncacheable(struct reg *reg)
{
	assert(!reg->dirty && "Non-cacheable register must never get dirty");
	return ERROR_OK;
}

static int get_vreg(struct reg *reg)
{
	assert(reg->number >= GDB_REGNO_V0 && reg->number <= GDB_REGNO_V31);
	return caching_get(reg, fetch_vreg);
}

static int set_vreg(struct reg *reg, uint8_t *buf)
{
	assert(reg->number >= GDB_REGNO_V0 && reg->number <= GDB_REGNO_V31);
	return caching_set(reg, buf, send_vreg);
}

static int flush_vreg(struct reg *reg)
{
	assert(reg->number >= GDB_REGNO_V0 && reg->number <= GDB_REGNO_V31);
	return flush_reg(reg, send_vreg);
}

static int get_pc(struct reg *pc)
{
	assert(pc->number == GDB_REGNO_PC);
	assert(!pc->valid);
	assert(!pc->dirty);
	struct reg * const dpc =
		riscv_reg_impl_cache_entry(riscv_reg_impl_get_target(pc),
				GDB_REGNO_DPC);
	int res = dpc->type->get(dpc);
	if (res == ERROR_OK) {
		/* "pc" is just an alias for "dpc". "dpc" just got updated, sync up "pc" */
		set_cache_value(pc, dpc->value);
	}
	return res;
}

static int set_pc(struct reg *pc, uint8_t *buf)
{
	assert(pc->number == GDB_REGNO_PC);
	assert(!pc->valid);
	assert(!pc->dirty);
	struct reg * const dpc =
		riscv_reg_impl_cache_entry(riscv_reg_impl_get_target(pc),
			GDB_REGNO_DPC);
	int res = dpc->type->set(dpc, buf);
	if (res == ERROR_OK) {
		/* "pc" is just an alias for "dpc". "dpc" just got updated, sync up "pc" */
		set_cache_value(pc, dpc->value);
	}
	return res;
}

static int get_priv(struct reg *priv)
{
	assert(priv->number == GDB_REGNO_PRIV);
	assert(!priv->valid);
	assert(!priv->dirty);
	struct reg * const dcsr =
		riscv_reg_impl_cache_entry(riscv_reg_impl_get_target(priv),
			GDB_REGNO_DCSR);
	assert(dcsr->size == 32);
	int res = dcsr->type->get(dcsr);
	if (res != ERROR_OK)
		return res;
	buf_set_u32(priv->value, VIRT_PRIV_PRV_OFFSET, VIRT_PRIV_PRV_LENGTH,
			buf_get_u32(dcsr->value, CSR_DCSR_PRV_OFFSET, CSR_DCSR_PRV_LENGTH));
	buf_set_u32(priv->value, VIRT_PRIV_V_OFFSET, VIRT_PRIV_V_LENGTH,
			buf_get_u32(dcsr->value, CSR_DCSR_V_OFFSET, CSR_DCSR_V_LENGTH));
	return ERROR_OK;
}

static int set_priv(struct reg *priv, uint8_t *priv_buf)
{
	assert(priv->number == GDB_REGNO_PRIV);
	assert(!priv->valid);
	assert(!priv->dirty);
	struct reg * const dcsr = riscv_reg_impl_cache_entry(riscv_reg_impl_get_target(priv),
			GDB_REGNO_DCSR);
	int res = dcsr->type->get(dcsr);
	if (res != ERROR_OK)
		return res;
	assert(dcsr->size == 32);
	uint8_t dcsr_buf[32 / 8];
	buf_cpy(dcsr->value, dcsr_buf, dcsr->size);
	buf_set_u32(dcsr_buf, CSR_DCSR_PRV_OFFSET, CSR_DCSR_PRV_LENGTH,
			buf_get_u32(priv_buf, VIRT_PRIV_PRV_OFFSET, VIRT_PRIV_PRV_LENGTH));
	buf_set_u32(dcsr_buf, CSR_DCSR_V_OFFSET, CSR_DCSR_V_LENGTH,
			buf_get_u32(priv_buf, VIRT_PRIV_V_OFFSET, VIRT_PRIV_V_LENGTH));
	res = dcsr->type->set(dcsr, dcsr_buf);
	if (res == ERROR_OK)
		set_cache_value(priv, priv_buf);
	return res;
}

/* End of "reg_arch_type" method definitions. */

static const struct reg_arch_type zero_type = {
	.get = riscv013_caching_get,
	/* "zero" is read-only, but there is nothig wrong with attempting to
	 * write it (e.g. for the purpose of HW testing). */
	.set = riscv013_noncaching_set,
	.flush = riscv013_reg_flush,
};

static const struct reg_arch_type xreg_type = {
	.get = riscv013_caching_get,
	.set = riscv013_caching_set,
	.flush = riscv013_reg_flush,
};

static const struct reg_arch_type freg_type = {
	.get = riscv013_caching_get,
	.set = riscv013_caching_set,
	.flush = riscv013_reg_flush,
};

static const struct reg_arch_type vreg_type = {
	.get = get_vreg,
	.set = set_vreg,
	.flush = flush_vreg
};

static const struct reg_arch_type pc_type = {
	.get = get_pc,
	.set = set_pc,
	.flush = flush_noncacheable
};

static const struct reg_arch_type priv_type = {
	.get = get_priv,
	.set = set_priv,
	.flush = flush_noncacheable
};

static const struct reg_arch_type warl_csr_type = {
	.get = riscv013_caching_get,
	.set = riscv013_noncaching_set,
	.flush = riscv013_reg_flush
};

static const struct reg_arch_type noncacheable_type = {
	.get = riscv013_noncaching_get,
	.set = riscv013_noncaching_set,
	.flush = flush_noncacheable
};

static const struct reg_arch_type *riscv013_gdb_regno_reg_type(uint32_t regno)
{
	if (regno != GDB_REGNO_ZERO && regno <= GDB_REGNO_XPR31)
		return &xreg_type;
	if (regno >= GDB_REGNO_FPR0 && regno <= GDB_REGNO_FPR31) {
		/* For now "freg_type" is the same as "xreg_type", but it will be
		 * changed soon */
		return &freg_type;
	}
	if (regno >= GDB_REGNO_V0 && regno <= GDB_REGNO_V31)
		return &vreg_type;
	switch (regno) {
	case GDB_REGNO_ZERO:
		return &zero_type;
	case GDB_REGNO_PC:
		return &pc_type;
	case GDB_REGNO_PRIV:
		return &priv_type;
	case GDB_REGNO_VLENB:
	/* "vlenb" is read-only, but there is nothig wrong with attempting to
	 * write it (e.g. for the purpose of HW testing). */
	case GDB_REGNO_DPC:
	case GDB_REGNO_VSTART:
	case GDB_REGNO_VXSAT:
	case GDB_REGNO_VXRM:
	case GDB_REGNO_VL:
	case GDB_REGNO_VTYPE:
	case GDB_REGNO_MISA:
	case GDB_REGNO_DCSR:
	case GDB_REGNO_DSCRATCH0:
	case GDB_REGNO_MEPC:
	case GDB_REGNO_SATP:
		return &warl_csr_type;
	/* TODO: https://github.com/riscv-collab/riscv-openocd/issues/1219 */
	case GDB_REGNO_MSTATUS:
	case GDB_REGNO_MCAUSE:
	/* TODO: Sdtrig register should be WARL. Note that "tdata*" and "tinfo"
	 * values must get invalidated when "tselect" changes. */
	case GDB_REGNO_TSELECT:
	case GDB_REGNO_TDATA1:
	case GDB_REGNO_TDATA2:
	default:
		return &noncacheable_type;
	}
}

static int init_cache_entry(struct target *target, uint32_t regno)
{
	struct reg * const reg = riscv_reg_impl_cache_entry(target, regno);
	if (riscv_reg_impl_is_initialized(reg))
		return ERROR_OK;
	return riscv_reg_impl_init_cache_entry(target, regno,
			riscv_reg_impl_gdb_regno_exist(target, regno),
			riscv013_gdb_regno_reg_type(regno));
}

/**
 * Some registers are optional (e.g. "misa"). For such registers it is first
 * assumed they exist (via "assume_reg_exist()"), then the read is attempted
 * (via the usual "riscv_reg_get()") and if the read fails, the register is
 * marked as non-existing (via "riscv_reg_impl_set_exist()").
 */
static int assume_reg_exist(struct target *target, uint32_t regno)
{
	return riscv_reg_impl_init_cache_entry(target, regno,
			/* exist */ true, riscv013_gdb_regno_reg_type(regno));
}

static int examine_xlen(struct target *target)
{
	RISCV_INFO(r);
	unsigned int cmderr;

	const uint32_t command = riscv013_access_register_command(target,
			GDB_REGNO_S0, /* size */ 64, AC_ACCESS_REGISTER_TRANSFER);
	int res = riscv013_execute_abstract_command(target, command, &cmderr);
	if (res == ERROR_OK) {
		r->xlen = 64;
		return ERROR_OK;
	}
	if (res == ERROR_TIMEOUT_REACHED)
		return ERROR_FAIL;
	r->xlen = 32;

	return ERROR_OK;
}

static int examine_vlenb(struct target *target)
{
	RISCV_INFO(r);

	/* Reading "vlenb" requires "mstatus.vs" to be set, so "mstatus" should
	 * be accessible.*/
	int res = init_cache_entry(target, GDB_REGNO_MSTATUS);
	if (res != ERROR_OK)
		return res;

	res = assume_reg_exist(target, GDB_REGNO_VLENB);
	if (res != ERROR_OK)
		return res;

	riscv_reg_t vlenb_val;
	if (riscv_reg_get(target, &vlenb_val, GDB_REGNO_VLENB) != ERROR_OK) {
		if (riscv_supports_extension(target, 'V'))
			LOG_TARGET_WARNING(target, "Couldn't read vlenb; vector register access won't work.");
		r->vlenb = 0;
		return riscv_reg_impl_set_exist(target, GDB_REGNO_VLENB, false);
	}
	/* As defined by RISC-V V extension specification:
	 * https://github.com/riscv/riscv-v-spec/blob/2f68ef7256d6ec53e4d2bd7cb12862f406d64e34/v-spec.adoc?plain=1#L67-L72 */
	const unsigned int vlen_max = 65536;
	const unsigned int vlenb_max = vlen_max / 8;
	if (vlenb_val > vlenb_max) {
		LOG_TARGET_WARNING(target, "'vlenb == %" PRIu64
				"' is greater than maximum allowed by specification (%u); vector register access won't work.",
				vlenb_val, vlenb_max);
		r->vlenb = 0;
		return ERROR_OK;
	}
	assert(vlenb_max <= UINT_MAX);
	r->vlenb = (unsigned int)vlenb_val;

	LOG_TARGET_INFO(target, "Vector support with vlenb=%u", r->vlenb);
	return ERROR_OK;
}

enum misa_mxl {
	MISA_MXL_INVALID = 0,
	MISA_MXL_32 = 1,
	MISA_MXL_64 = 2,
	MISA_MXL_128 = 3
};

unsigned int mxl_to_xlen(enum misa_mxl mxl)
{
	switch (mxl) {
	case MISA_MXL_32:
		return 32;
	case MISA_MXL_64:
		return 64;
	case MISA_MXL_128:
		return 128;
	case MISA_MXL_INVALID:
		assert(0);
	}
	return 0;
}

static int check_misa_mxl(const struct target *target)
{
	RISCV_INFO(r);

	if (r->misa == 0) {
		LOG_TARGET_WARNING(target, "'misa' register is read as zero."
				"OpenOCD will not be able to determine some hart's capabilities.");
		return ERROR_OK;
	}
	const unsigned int dxlen = riscv_xlen(target);
	assert(dxlen <= sizeof(riscv_reg_t) * CHAR_BIT);
	assert(dxlen >= 2);
	const riscv_reg_t misa_mxl_mask = (riscv_reg_t)0x3 << (dxlen - 2);
	const unsigned int mxl = get_field(r->misa, misa_mxl_mask);
	if (mxl == MISA_MXL_INVALID) {
		/* This is not an error!
		 * Imagine the platform that:
		 * - Has no abstract access to CSRs, so that CSRs are read
		 *   through Program Buffer via "csrr" instruction.
		 * - Complies to v1.10 of the Priveleged Spec, so that misa.mxl
		 *   is WARL and MXLEN may be chainged.
		 *   https://github.com/riscv/riscv-isa-manual/commit/9a7dd2fe29011587954560b5dcf1875477b27ad8
		 * - DXLEN == MXLEN on reset == 64.
		 * In a following scenario:
		 * - misa.mxl was written, so that MXLEN is 32.
		 * - Debugger connects to the target.
		 * - Debugger observes DXLEN == 64.
		 * - Debugger reads misa:
		 *   - Abstract access fails with "cmderr == not supported".
		 *   - Access via Program Buffer involves reading "misa" to an
		 *     "xreg" via "csrr", so that the "xreg" is filled with
		 *     zero-extended value of "misa" (since "misa" is
		 *     MXLEN-wide).
		 * - Debugger derives "misa.mxl" assumig "misa" is DXLEN-bit
		 *   wide (64) while MXLEN is 32 and therefore erroneously
		 *   assumes "misa.mxl" to be zero (invalid).
		 */
		LOG_TARGET_WARNING(target, "Detected DXLEN (%u) does not match "
				"MXLEN: misa.mxl == 0, misa == 0x%" PRIx64 ".",
				dxlen, r->misa);
		return ERROR_OK;
	}
	const unsigned int mxlen = mxl_to_xlen(mxl);
	if (dxlen < mxlen) {
		LOG_TARGET_ERROR(target,
				"MXLEN (%u) reported in misa.mxl field exceeds "
				"the detected DXLEN (%u)",
				mxlen, dxlen);
		return ERROR_FAIL;
	}
	/* NOTE:
	 * The value of "misa.mxl" may stil not coincide with "xlen".
	 * "misa[26:XLEN-3]" bits are marked as WIRI in at least version 1.10
	 * of RISC-V Priveleged Spec. Therefore, if "xlen" is erroneously
	 * assumed to be 32 when it actually is 64, "mxl" will be read from
	 * this WIRI field and may be equal to "MISA_MXL_32" by coincidence.
	 * This is not an issue though from the version 1.11 onward, since
	 * "misa[26:XLEN-3]" became WARL and equal to 0.
	 */

	/* Display this as early as possible to help people who are using
	 * really slow simulators. */
	LOG_TARGET_DEBUG(target, " XLEN=%d, misa=0x%" PRIx64, riscv_xlen(target), r->misa);
	return ERROR_OK;
}

static int examine_misa(struct target *target)
{
	RISCV_INFO(r);

	int res = init_cache_entry(target, GDB_REGNO_MISA);
	if (res != ERROR_OK)
		return res;

	res = riscv_reg_get(target, &r->misa, GDB_REGNO_MISA);
	if (res != ERROR_OK)
		return res;
	return check_misa_mxl(target);
}

static int examine_mtopi(struct target *target)
{
	/* Assume the registers exist */
	int res = assume_reg_exist(target, GDB_REGNO_MTOPI);
	if (res != ERROR_OK)
		return res;
	res = assume_reg_exist(target, GDB_REGNO_MTOPEI);
	if (res != ERROR_OK)
		return res;

	riscv_reg_t value;
	if (riscv_reg_get(target, &value, GDB_REGNO_MTOPI) != ERROR_OK) {
		res = riscv_reg_impl_set_exist(target, GDB_REGNO_MTOPI, false);
		if (res != ERROR_OK)
			return res;
		return riscv_reg_impl_set_exist(target, GDB_REGNO_MTOPEI, false);
	}
	if (riscv_reg_get(target, &value, GDB_REGNO_MTOPEI) != ERROR_OK) {
		LOG_TARGET_INFO(target, "S?aia detected without IMSIC");
		return riscv_reg_impl_set_exist(target, GDB_REGNO_MTOPEI, false);
	}
	LOG_TARGET_INFO(target, "S?aia detected with IMSIC");
	return ERROR_OK;
}

/**
 * This function assumes target's DM to be initialized (target is able to
 * access DMs registers, execute program buffer, etc.)
 */
int riscv013_reg_examine_all(struct target *target)
{
	int res = riscv_reg_impl_init_cache(target);
	if (res != ERROR_OK)
		return res;

	init_shared_reg_info(target);

	assert(target->state == TARGET_HALTED);

	res = examine_xlen(target);
	if (res != ERROR_OK)
		return res;

	/* Reading CSRs may clobber "s0", "s1", so it should be possible to
	 * save them in cache. */
	res = init_cache_entry(target, GDB_REGNO_S0);
	if (res != ERROR_OK)
		return res;
	res = init_cache_entry(target, GDB_REGNO_S1);
	if (res != ERROR_OK)
		return res;

	res = examine_misa(target);
	if (res != ERROR_OK)
		return res;

	res = examine_vlenb(target);
	if (res != ERROR_OK)
		return res;

	riscv_reg_impl_init_vector_reg_type(target);

	res = examine_mtopi(target);
	if (res != ERROR_OK)
		return res;

	for (uint32_t regno = 0; regno < target->reg_cache->num_regs; ++regno) {
		res = init_cache_entry(target, regno);
		if (res != ERROR_OK)
			return res;
	}

	res = riscv_reg_impl_expose_csrs(target);
	if (res != ERROR_OK)
		return res;

	riscv_reg_impl_hide_csrs(target);

	return ERROR_OK;
}

int riscv013_reg_save_gpr(struct target *target, enum gdb_regno regid)
{
	assert(regid >= GDB_REGNO_ZERO && regid <= GDB_REGNO_XPR31);
	struct reg *reg = riscv_reg_impl_cache_entry(target, regid);
	assert(riscv_reg_impl_is_initialized(reg));

	RISCV_INFO(r);
	if (target->state != TARGET_HALTED) {
		LOG_TARGET_ERROR(target, "Can't save register %s on a hart that is not halted",
				reg->name);
		return ERROR_TARGET_NOT_HALTED;
	}

	LOG_TARGET_DEBUG(target, "Saving %s", reg->name);
	int res = reg->type->get(reg);
	if (res != ERROR_OK)
		return res;

	assert(reg->valid &&
			"The register is cacheable, so the cache entry must be valid now.");
	/* Mark the register as dirty so that its value is written back to the target
	 * during the next register cache flush. We assume that this function is called
	 * because the caller is about to mess with the underlying value of the
	 * register, and wants to make sure the value gets restored later. */
	reg->dirty = true;

	r->last_activity = timeval_ms();

	return ERROR_OK;
}
