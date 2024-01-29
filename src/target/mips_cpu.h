/* SPDX-License-Identifier: GPL-2.0 */
#ifndef OPENOCD_TARGET_MIPS_CPU_H
#define OPENOCD_TARGET_MIPS_CPU_H

/*
 * NOTE: The proper detection of certain CPUs can become quite complicated.
 * Please consult the following Linux kernel code when adding new CPUs:
 *  arch/mips/include/asm/cpu.h
 *  arch/mips/kernel/cpu-probe.c
 */

/* Assigned Company values for bits 23:16 of the PRId register. */
#define	PRID_COMP_MASK		0xff0000

#define	PRID_COMP_LEGACY    0x000000
#define	PRID_COMP_MTI       0x010000
#define PRID_COMP_BROADCOM  0x020000
#define PRID_COMP_ALCHEMY   0x030000
#define PRID_COMP_LEXRA     0x0b0000
#define PRID_COMP_ALTERA    0x100000
#define	PRID_COMP_INGENIC_E1	0xe10000

/*
 * Assigned Processor ID (implementation) values for bits 15:8 of the PRId
 * register. In order to detect a certain CPU type exactly eventually additional
 * registers may need to be examined.
 */
#define PRID_IMP_MASK		0xff00

#define PRID_IMP_MAPTIV_UC      0x9D00
#define PRID_IMP_MAPTIV_UP      0x9E00
#define PRID_IMP_IAPTIV_CM      0xA000
#define PRID_IMP_IAPTIV         0xA100
#define PRID_IMP_M5150          0xA700

#define PRID_IMP_XBURST_REV1	0x0200 /* XBurst®1 with MXU1.0/MXU1.1 SIMD ISA */

#endif /* OPENOCD_TARGET_MIPS_CPU_H */
