// SPDX-License-Identifier: GPL-2.0-or-later

/***************************************************************************
 *   Copyright (C) 2005 by Dominic Rath                                    *
 *   Dominic.Rath@gmx.de                                                   *
 *                                                                         *
 *   Copyright (C) 2007,2008 Øyvind Harboe                                 *
 *   oyvind.harboe@zylin.com                                               *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "register.h"
#include <helper/log.h>
#include <target/target.h>
#include <target/target_type.h>

/**
 * @file
 * Holds utilities to work with register caches.
 *
 * OpenOCD uses machine registers internally, and exposes them by name
 * to Tcl scripts.  Sets of related registers are grouped into caches.
 * For example, a CPU core will expose a set of registers, and there
 * may be separate registers associated with debug or trace modules.
 */

struct reg *register_get_by_number(struct reg_cache *first,
		uint32_t reg_num, bool search_all)
{
	struct reg_cache *cache = first;

	while (cache) {
		for (unsigned int i = 0; i < cache->num_regs; i++) {
			if (!cache->reg_list[i].exist)
				continue;
			if (cache->reg_list[i].number == reg_num)
				return &(cache->reg_list[i]);
		}

		if (!search_all)
			break;

		cache = cache->next;
	}

	return NULL;
}

struct reg *register_get_by_name(struct reg_cache *first,
		const char *name, bool search_all)
{
	struct reg_cache *cache = first;

	while (cache) {
		for (unsigned int i = 0; i < cache->num_regs; i++) {
			if (!cache->reg_list[i].exist)
				continue;
			if (strcmp(cache->reg_list[i].name, name) == 0)
				return &(cache->reg_list[i]);
		}

		if (!search_all)
			break;

		cache = cache->next;
	}

	return NULL;
}

struct reg_cache **register_get_last_cache_p(struct reg_cache **first)
{
	struct reg_cache **cache_p = first;

	if (*cache_p)
		while (*cache_p)
			cache_p = &((*cache_p)->next);
	else
		return first;

	return cache_p;
}

void register_unlink_cache(struct reg_cache **cache_p, const struct reg_cache *cache)
{
	while (*cache_p && *cache_p != cache)
		cache_p = &((*cache_p)->next);
	if (*cache_p)
		*cache_p = cache->next;
}

/** Marks the contents of the register cache as invalid (and clean). */
void register_cache_invalidate(struct reg_cache *cache)
{
	for (unsigned int n = 0; n < cache->num_regs; n++) {
		struct reg *reg = &cache->reg_list[n];
		if (!reg->exist)
			continue;
		reg->valid = false;
		reg->dirty = false;
	}
}

static int register_get_dummy_core_reg(struct reg *reg)
{
	return ERROR_OK;
}

static int register_set_dummy_core_reg(struct reg *reg, uint8_t *buf)
{
	reg->dirty = true;
	reg->valid = true;

	return ERROR_OK;
}

static int register_flush_dummy(struct reg *reg)
{
	reg->dirty = false;

	return ERROR_OK;
}

static const struct reg_arch_type dummy_type = {
	.get = register_get_dummy_core_reg,
	.set = register_set_dummy_core_reg,
	.flush = register_flush_dummy,
};

void register_init_dummy(struct reg *reg)
{
	reg->type = &dummy_type;
}

int register_flush(const struct target *target, struct reg *reg, bool invalidate)
{
	if (!reg) {
		LOG_ERROR("BUG: %s called with NULL", __func__);
		return ERROR_FAIL;
	}

	if (!reg->exist) {
		LOG_ERROR("BUG: %s called with non-existent register", __func__);
		return ERROR_FAIL;
	}

	if (!reg->dirty) {
		LOG_TARGET_DEBUG(target, "Register '%s' is not dirty, nothing to flush", reg->name);
		if (reg->valid && invalidate) {
			LOG_TARGET_DEBUG(target, "Invalidating register '%s'", reg->name);
			reg->valid = false;
		}
		return ERROR_OK;
	}

	if (!reg->type->flush) {
		LOG_TARGET_ERROR(target, "Unable to flush dirty register '%s' - operation not yet supported "
			"by %s implementation in OpenOCD", reg->name, target->type->name);
		return ERROR_NOT_IMPLEMENTED;
	}

	if (!reg->valid) {
		LOG_ERROR("BUG: Register '%s' is not valid, but flush attempted", reg->name);
		return ERROR_FAIL;
	}

	LOG_TARGET_DEBUG(target, "Flushing register '%s'", reg->name);

	int result = reg->type->flush(reg);
	if (result != ERROR_OK) {
		LOG_TARGET_ERROR(target, "Failed to flush register '%s'", reg->name);
		return result;
	}

	if (reg->dirty) {
		LOG_ERROR("BUG: Register '%s' remained dirty after flushing", reg->name);
		return ERROR_FAIL;
	}
	if (reg->valid && invalidate) {
		LOG_TARGET_DEBUG(target, "Invalidating register '%s' after flush", reg->name);
		reg->valid = false;
	}

	return ERROR_OK;
}
