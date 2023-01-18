// SPDX-License-Identifier: GPL-2.0-or-later

/***************************************************************************
 *   Copyright (C) 2005 by Dominic Rath                                    *
 *   Dominic.Rath@gmx.de                                                   *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "armv4_5_cache.h"
#include <helper/log.h>

int armv4_5_identify_cache(uint32_t cache_type_reg, struct armv4_5_cache_common *cache)
{
	int size, assoc, M, len, multiplier;

	cache->ctype = (cache_type_reg & 0x1e000000U) >> 25;
	cache->separate = (cache_type_reg & 0x01000000U) >> 24;

	size = (cache_type_reg & 0x1c0000) >> 18;
	assoc = (cache_type_reg & 0x38000) >> 15;
	M = (cache_type_reg & 0x4000) >> 14;
	len = (cache_type_reg & 0x3000) >> 12;
	multiplier = 2 + M;

	if ((assoc != 0) || (M != 1)) /* assoc 0 and M 1 means cache absent */ {
		/* cache is present */
		cache->d_u_size.linelen = 1 << (len + 3);
		cache->d_u_size.associativity = multiplier << (assoc - 1);
		cache->d_u_size.nsets = 1 << (size + 6 - assoc - len);
		cache->d_u_size.cachesize = multiplier << (size + 8);
	} else {
		/* cache is absent */
		cache->d_u_size.linelen = -1;
		cache->d_u_size.associativity = -1;
		cache->d_u_size.nsets = -1;
		cache->d_u_size.cachesize = -1;
	}

	if (cache->separate) {
		size = (cache_type_reg & 0x1c0) >> 6;
		assoc = (cache_type_reg & 0x38) >> 3;
		M = (cache_type_reg & 0x4) >> 2;
		len = (cache_type_reg & 0x3);
		multiplier = 2 + M;

		if ((assoc != 0) || (M != 1)) /* assoc 0 and M 1 means cache absent */ {
			/* cache is present */
			cache->i_size.linelen = 1 << (len + 3);
			cache->i_size.associativity = multiplier << (assoc - 1);
			cache->i_size.nsets = 1 << (size + 6 - assoc - len);
			cache->i_size.cachesize = multiplier << (size + 8);
		} else {
			/* cache is absent */
			cache->i_size.linelen = -1;
			cache->i_size.associativity = -1;
			cache->i_size.nsets = -1;
			cache->i_size.cachesize = -1;
		}
	} else
		cache->i_size = cache->d_u_size;

	return ERROR_OK;
}

int armv4_5_handle_cache_info_command(struct command_invocation *cmd, struct armv4_5_cache_common *armv4_5_cache)
{
	if (armv4_5_cache->ctype == -1) {
		command_print(cmd, "cache not yet identified");
		return ERROR_OK;
	}

	command_print(cmd, "cache type: 0x%1.1x, %s", armv4_5_cache->ctype,
		(armv4_5_cache->separate) ? "separate caches" : "unified cache");

	command_print(cmd, "D-Cache: linelen %i, associativity %i, nsets %i, cachesize 0x%x",
		armv4_5_cache->d_u_size.linelen,
		armv4_5_cache->d_u_size.associativity,
		armv4_5_cache->d_u_size.nsets,
		armv4_5_cache->d_u_size.cachesize);

	command_print(cmd, "I-Cache: linelen %i, associativity %i, nsets %i, cachesize 0x%x",
		armv4_5_cache->i_size.linelen,
		armv4_5_cache->i_size.associativity,
		armv4_5_cache->i_size.nsets,
		armv4_5_cache->i_size.cachesize);

	return ERROR_OK;
}
