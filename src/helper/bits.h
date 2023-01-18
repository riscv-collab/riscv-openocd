/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2018, STMicroelectronics - All Rights Reserved
 * Author(s): Antonio Borneo <borneo.antonio@gmail.com> for STMicroelectronics
 */

/*
 * The content of this file is mainly copied/inspired from Linux kernel
 * code in include/linux/types.h include/linux/bitmap.h include/linux/bitops.h
 */

#ifndef OPENOCD_HELPER_BITS_H
#define OPENOCD_HELPER_BITS_H

#include <helper/replacements.h>
#include <helper/types.h>

#define BIT(nr)                     (1UL << (nr))
#define BIT_ULL(nr)                 (1ULL << (nr))
#define BITS_PER_BYTE               8
#define BITS_PER_LONG               (BITS_PER_BYTE * sizeof(long))
#define BITS_PER_LONG_LONG          (BITS_PER_BYTE * sizeof(long long))
#define GENMASK(h, l)               (((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#define GENMASK_ULL(h, l)           (((~0ULL) - (1ULL << (l)) + 1) & (~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))
#define BITS_TO_LONGS(nr)           DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))
#define BIT_MASK(nr)                (1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)                ((nr) / BITS_PER_LONG)
#define DECLARE_BITMAP(name, bits)  unsigned long name[BITS_TO_LONGS(bits)]

/**
 * bitmap_zero - Clears all the bits in memory
 * @param dst the address of the bitmap
 * @param nbits the number of bits to clear
 */
static inline void bitmap_zero(unsigned long *dst, unsigned int nbits)
{
	unsigned int len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
	memset(dst, 0, len);
}

/**
 * clear_bit - Clear a bit in memory
 * @param nr the bit to set
 * @param addr the address to start counting from
 */
static inline void clear_bit(unsigned int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

	*p &= ~mask;
}

/**
 * set_bit - Set a bit in memory
 * @param nr the bit to set
 * @param addr the address to start counting from
 */
static inline void set_bit(unsigned int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

	*p |= mask;
}

/**
 * test_bit - Determine whether a bit is set
 * @param nr bit number to test
 * @param addr Address to start counting from
 */
static inline int test_bit(unsigned int nr, const volatile unsigned long *addr)
{
	return 1UL & (addr[BIT_WORD(nr)] >> (nr % BITS_PER_LONG));
}

#endif /* OPENOCD_HELPER_BITS_H */
