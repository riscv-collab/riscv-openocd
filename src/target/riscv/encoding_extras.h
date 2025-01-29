/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * This file contains additional constants and macros in the same format as the generated
 * encoding.h header. These are intended to supplement the headers contents, which are up-to-date
 * with latest RISC-V specifications, with support for select features that were deprecated,
 * but already have existing hardware implementations.
 */

#ifndef RISCV_ENCODING_EXTRAS_H
#define RISCV_ENCODING_EXTRAS_H
#define CSR_USTATUS 0x0
#define CSR_UIE 0x4
#define CSR_UTVEC 0x5
#define CSR_USCRATCH 0x40
#define CSR_UEPC 0x41
#define CSR_UCAUSE 0x42
#define CSR_UTVAL 0x43
#define CSR_UIP 0x44
#endif // RISCV_ENCODING_EXTRAS_H

#ifdef DECLARE_CSR
DECLARE_CSR(ustatus, CSR_USTATUS)
DECLARE_CSR(uie, CSR_UIE)
DECLARE_CSR(utvec, CSR_UTVEC)
DECLARE_CSR(uscratch, CSR_USCRATCH)
DECLARE_CSR(uepc, CSR_UEPC)
DECLARE_CSR(ucause, CSR_UCAUSE)
DECLARE_CSR(utval, CSR_UTVAL)
DECLARE_CSR(uip, CSR_UIP)
#endif // DECLARE_CSR
