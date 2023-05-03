// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <endian.h>
#include <unistd.h>

#include "axi_hw.h"

#define LSCU_DELAY_US 10
#define LSCU_TIMEOUT 1000

#define AXI_DELAY_US 10
#define AXI_TIMEOUT 1000

/* #define DEBUG_AXI_HW */

static int axi_tap_lscu_access_read(struct jtag_tap *tap, uint8_t *data)
{
	uint8_t tap_addr = AXI_TAP__LSCU_ACCESS__ADDR;

	return tap_reg_read(tap, &tap_addr, AXI_TAP__LSCU_ACCESS__WIDTH, data);
}

static int axi_tap_lscu_access_write(struct jtag_tap *tap, uint8_t *data)
{
	uint8_t tap_addr = AXI_TAP__LSCU_ACCESS__ADDR;

	return tap_reg_write(tap, &tap_addr, AXI_TAP__LSCU_ACCESS__WIDTH, data);
}

int axi_lscu_reg_read(struct jtag_tap *tap, uint8_t addr, uint32_t *value)
{
	int ret;

	uint64_t reg = 0;
	uint64_t reg_le;
	uint8_t *data;
	int count = 0;

	REG64_SET_FIELD(&reg, AXI_TAP__LSCU_ACCESS, OPERATION, AXI_TAP__LSCU_ACCESS__OPERATION__VALUE__READ);
	REG64_SET_FIELD(&reg, AXI_TAP__LSCU_ACCESS, ADDRESS, addr);

	reg_le = htole64(reg);
	data = (uint8_t *)&reg_le;

	ret = axi_tap_lscu_access_write(tap, data);
	if (ret != ERROR_OK)
		return ret;

	do {
		uint8_t status;
		reg_le = 0;

		ret = axi_tap_lscu_access_read(tap, data);
		if (ret != ERROR_OK)
			return ret;

		reg = le64toh(reg_le);

		*value = (uint32_t)REG64_GET_FIELD(reg, AXI_TAP__LSCU_ACCESS, DATA);
		status = (uint8_t)REG64_GET_FIELD(reg, AXI_TAP__LSCU_ACCESS, STATUS);

		if (status == AXI_TAP__LSCU_ACCESS__STATUS__VALUE__OKAY)
			break;

		if (status == AXI_TAP__LSCU_ACCESS__STATUS__VALUE__ERR)
			return ERROR_FAIL;

		if (count >= LSCU_TIMEOUT)
			return ERROR_TIMEOUT_REACHED;

		usleep(LSCU_DELAY_US);
		count++;
	} while (1);

#ifdef DEBUG_AXI_HW
	printf("AXI LSCU read  0x%02X: 0x%08X\n", addr, *value);
#endif

	return ERROR_OK;
}

int axi_lscu_reg_write(struct jtag_tap *tap, uint8_t addr, uint32_t value)
{
	int ret;

	uint64_t reg = 0;
	uint64_t reg_le;
	uint8_t *data;
	int count = 0;

	REG64_SET_FIELD(&reg, AXI_TAP__LSCU_ACCESS, OPERATION, AXI_TAP__LSCU_ACCESS__OPERATION__VALUE__WRITE);
	REG64_SET_FIELD(&reg, AXI_TAP__LSCU_ACCESS, ADDRESS, addr);
	REG64_SET_FIELD(&reg, AXI_TAP__LSCU_ACCESS, DATA, value);

	reg_le = htole64(reg);
	data = (uint8_t *)&reg_le;

	ret = axi_tap_lscu_access_write(tap, data);
	if (ret != ERROR_OK)
		return ret;

	do {
		uint8_t status;
		reg_le = 0;

		ret = axi_tap_lscu_access_read(tap, data);
		if (ret != ERROR_OK)
			return ret;

		reg = le64toh(reg_le);

		status = (uint8_t)REG64_GET_FIELD(reg, AXI_TAP__LSCU_ACCESS, STATUS);

		if (status == AXI_TAP__LSCU_ACCESS__STATUS__VALUE__OKAY)
			break;

		if (status == AXI_TAP__LSCU_ACCESS__STATUS__VALUE__ERR)
			return ERROR_FAIL;

		if (count >= LSCU_TIMEOUT)
			return ERROR_TIMEOUT_REACHED;

		usleep(LSCU_DELAY_US);
		count++;
	} while (1);

#ifdef DEBUG_AXI_HW
	printf("AXI LSCU write 0x%02X: 0x%08X\n", addr, value);
#endif

	return ERROR_OK;
}

int axi_single_read_transaction(struct jtag_tap *tap, uint64_t addr, uint32_t *data, unsigned int len)
{
	int ret;

	uint32_t reg;
	unsigned int count, i;
	uint32_t rdata[AXI_AXI_DW / 32];

	ret = axi_lscu_reg_write(tap, AXI_LSCU__CMD__ADDR, 0);
	if (ret != ERROR_OK)
		return ret;

	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXID__ADDR, 1);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXADDR__ADDR, addr & 0x00000000FFFFFFFFULL);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXADDR__ADDR, (addr & 0x000000FF00000000ULL) >> 32);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXLEN__ADDR, len - 1);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXSIZE__ADDR, AXI_LSCU__AXSIZE__AXSIZE__VALUE__4_BYTES);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXBURST__ADDR, AXI_LSCU__AXBURST__AXBURST__VALUE__INCR);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXLOCK__ADDR, 0);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXCACHE__ADDR, 0);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXPROT__ADDR, 0);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXQOS__ADDR, 0);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXREGION__ADDR, 0);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__ARUSER__ADDR, 0);
	if (ret != ERROR_OK)
		return ret;

	count = 0;
	do {
		ret = axi_lscu_reg_read(tap, AXI_LSCU__STATUS__ADDR, &reg);
		if (ret != ERROR_OK)
			return ret;

		if (REG32_GET_FIELD(reg, AXI_LSCU__STATUS, AX_FIFO_FULL) == 0)
			break;

		if (count >= AXI_TIMEOUT) {
			/* Error: JTAG2AXIM us busy, AX FIFO is full */
			return ERROR_WAIT;
		}

		usleep(AXI_DELAY_US);
		count++;
	} while (1);

	reg = 0;
	REG32_SET_FIELD(&reg, AXI_LSCU__CMD, REQ, AXI_LSCU__CMD__REQ__VALUE__REQ_RD);
	ret = axi_lscu_reg_write(tap, AXI_LSCU__CMD__ADDR, reg);
	if (ret != ERROR_OK)
		return ret;

	for (i = 0; i < len; i++) {
		count = 0;
		do {
			ret = axi_lscu_reg_read(tap, AXI_LSCU__STATUS__ADDR, &reg);
			if (ret != ERROR_OK)
				goto release_r;

			if (REG32_GET_FIELD(reg, AXI_LSCU__STATUS, R_FIFO_EMPTY) == 0)
				break;

			if (count >= AXI_TIMEOUT) {
				/* Error: Didn't get a response */
				return ERROR_WAIT;
			}

			usleep(AXI_DELAY_US);
			count++;
		} while (1);

		ret = axi_lscu_reg_read(tap, AXI_LSCU__RID__ADDR, &reg);
		if (ret != ERROR_OK)
			goto release_r;
		if (reg != 1) {
			/* Error: Got an unexpected ID in response */
			ret = ERROR_FAIL;
			goto release_r;
		}

		ret = axi_lscu_reg_read(tap, AXI_LSCU__RRESP__ADDR, &reg);
		if (ret != ERROR_OK)
			goto release_r;
		if (reg != AXI_LSCU__RRESP__RRESP__VALUE__OKAY) {
			/* Error: Response status is not OKAY */
			ret = ERROR_FAIL;
			goto release_r;
		}

		ret = axi_lscu_reg_read(tap, AXI_LSCU__RLAST__ADDR, &reg);
		if (ret != ERROR_OK)
			goto release_r;
		if (i != len - 1) {
			if (reg != 0) {
				/* Error: RLAST should be asserted only for the last response */
				ret = ERROR_FAIL;
				goto release_r;
			}
		} else {
			if (reg != 1) {
				/* Error: RLAST should be asserted for the last response */
				ret = ERROR_FAIL;
				goto release_r;
			}
		}

		for (count = 0; count < AXI_AXI_DW / 32; count++) {
			ret = axi_lscu_reg_read(tap, AXI_LSCU__RDATA__ADDR, &rdata[count]);
			if (ret != ERROR_OK)
				goto release_r;
		}
		data[i] = rdata[((addr + i * 4) % (AXI_AXI_DW / 8)) / 4];

		ret = axi_lscu_reg_read(tap, AXI_LSCU__RUSER__ADDR, &reg);
		if (ret != ERROR_OK)
			goto release_r;

release_r:
		reg = 0;
		REG32_SET_FIELD(&reg, AXI_LSCU__CMD, REQ, AXI_LSCU__CMD__REQ__VALUE__RLSE_R);
		(void)axi_lscu_reg_write(tap, AXI_LSCU__CMD__ADDR, reg);
		if (ret != ERROR_OK)
			break;
	}

	/* Empty R FIFO */
	reg = 0;
	(void)axi_lscu_reg_read(tap, AXI_LSCU__STATUS__ADDR, &reg);
	if (REG32_GET_FIELD(reg, AXI_LSCU__STATUS, R_FIFO_EMPTY) == 0) {
		/* Error: R FIFO should be empty after reading last response */
		ret = ERROR_FAIL;

		count = 0;
		while (REG32_GET_FIELD(reg, AXI_LSCU__STATUS, R_FIFO_EMPTY) == 0 &&
			   count < 256) {
			reg = 0;
			REG32_SET_FIELD(&reg, AXI_LSCU__CMD, REQ, AXI_LSCU__CMD__REQ__VALUE__RLSE_R);
			(void)axi_lscu_reg_write(tap, AXI_LSCU__CMD__ADDR, reg);
			reg = 0;
			(void)axi_lscu_reg_read(tap, AXI_LSCU__STATUS__ADDR, &reg);

			count++;
		}
	}

	return ret;
}

int axi_single_write_transaction(struct jtag_tap *tap, uint64_t addr, uint32_t *data, unsigned int len)
{
	int ret;

	uint32_t reg;
	unsigned int count, i;
	uint32_t wdata[AXI_AXI_DW / 32];

	ret = axi_lscu_reg_write(tap, AXI_LSCU__CMD__ADDR, 0);
	if (ret != ERROR_OK)
		return ret;

	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXID__ADDR, 1);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXADDR__ADDR, addr & 0x00000000FFFFFFFFULL);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXADDR__ADDR, (addr & 0x000000FF00000000ULL) >> 32);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXLEN__ADDR, len - 1);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXSIZE__ADDR, AXI_LSCU__AXSIZE__AXSIZE__VALUE__4_BYTES);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXBURST__ADDR, AXI_LSCU__AXBURST__AXBURST__VALUE__INCR);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXLOCK__ADDR, 0);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXCACHE__ADDR, 0);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXPROT__ADDR, 0);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXQOS__ADDR, 0);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AXREGION__ADDR, 0);
	if (ret != ERROR_OK)
		return ret;
	ret = axi_lscu_reg_write(tap, AXI_LSCU__AWUSER__ADDR, 0);
	if (ret != ERROR_OK)
		return ret;

	count = 0;
	do {
		ret = axi_lscu_reg_read(tap, AXI_LSCU__STATUS__ADDR, &reg);
		if (ret != ERROR_OK)
			return ret;

		if (REG32_GET_FIELD(reg, AXI_LSCU__STATUS, AX_FIFO_FULL) == 0)
			break;

		if (count >= AXI_TIMEOUT) {
			/* Error: JTAG2AXIM us busy, AX FIFO is full */
			return ERROR_WAIT;
		}

		usleep(AXI_DELAY_US);
		count++;
	} while (1);

	reg = 0;
	REG32_SET_FIELD(&reg, AXI_LSCU__CMD, REQ, AXI_LSCU__CMD__REQ__VALUE__REQ_WR);
	ret = axi_lscu_reg_write(tap, AXI_LSCU__CMD__ADDR, reg);
	if (ret != ERROR_OK)
		return ret;

	for (i = 0; i < len; i++) {
		memset(wdata, 0, sizeof(wdata));
		wdata[((addr + i * 4) % (AXI_AXI_DW / 8)) / 4] = data[i];
		for (count = 0; count < AXI_AXI_DW / 32; count++) {
			ret = axi_lscu_reg_write(tap, AXI_LSCU__WDATA__ADDR, wdata[count]);
			if (ret != ERROR_OK)
				return ret;
		}
		ret = axi_lscu_reg_write(tap, AXI_LSCU__WSTRB__ADDR, 0xF << ((addr + i * 4) % (AXI_AXI_DW / 8)));
		if (ret != ERROR_OK)
			return ret;
		ret = axi_lscu_reg_write(tap, AXI_LSCU__WUSER__ADDR, 0);
		if (ret != ERROR_OK)
			return ret;

		count = 0;
		do {
			ret = axi_lscu_reg_read(tap, AXI_LSCU__STATUS__ADDR, &reg);
			if (ret != ERROR_OK)
				return ret;

			if (REG32_GET_FIELD(reg, AXI_LSCU__STATUS, W_FIFO_FULL) == 0)
				break;

			if (count >= AXI_TIMEOUT) {
				/* Error: JTAG2AXIM didn't push previous data from W FIFO */
				return ERROR_WAIT;
			}

			usleep(AXI_DELAY_US);
			count++;
		} while (1);

		reg = 0;
		if (i != len - 1)
			REG32_SET_FIELD(&reg, AXI_LSCU__CMD, BURST, AXI_LSCU__CMD__BURST__VALUE__PUSH_W);
		else
			REG32_SET_FIELD(&reg, AXI_LSCU__CMD, BURST, AXI_LSCU__CMD__BURST__VALUE__PUSH_W_LST);
		ret = axi_lscu_reg_write(tap, AXI_LSCU__CMD__ADDR, reg);
		if (ret != ERROR_OK)
			return ret;
	}

	count = 0;
	do {
		ret = axi_lscu_reg_read(tap, AXI_LSCU__STATUS__ADDR, &reg);
		if (ret != ERROR_OK)
			return ret;

		if (REG32_GET_FIELD(reg, AXI_LSCU__STATUS, B_FIFO_EMPTY) == 0)
			break;

		if (count >= AXI_TIMEOUT) {
			/* Error: Didn't get a response */
			return ERROR_WAIT;
		}

		usleep(AXI_DELAY_US);
		count++;
	} while (1);

	ret = axi_lscu_reg_read(tap, AXI_LSCU__BID__ADDR, &reg);
	if (ret != ERROR_OK)
		goto release_b;
	if (reg != 1) {
		/* Error: Got an unexpected ID in response */
		ret = ERROR_FAIL;
		goto release_b;
	}

	ret = axi_lscu_reg_read(tap, AXI_LSCU__BRESP__ADDR, &reg);
	if (ret != ERROR_OK)
		goto release_b;
	if (reg != AXI_LSCU__BRESP__BRESP__VALUE__OKAY) {
		/* Error: Response status is not OKAY */
		ret = ERROR_FAIL;
		goto release_b;
	}

	ret = axi_lscu_reg_read(tap, AXI_LSCU__BUSER__ADDR, &reg);
	if (ret != ERROR_OK)
		goto release_b;

release_b:
	reg = 0;
	REG32_SET_FIELD(&reg, AXI_LSCU__CMD, REQ, AXI_LSCU__CMD__REQ__VALUE__RLSE_B);
	(void)axi_lscu_reg_write(tap, AXI_LSCU__CMD__ADDR, reg);

	/* Empty B FIFO */
	reg = 0;
	(void)axi_lscu_reg_read(tap, AXI_LSCU__STATUS__ADDR, &reg);
	if (REG32_GET_FIELD(reg, AXI_LSCU__STATUS, B_FIFO_EMPTY) == 0) {
		/* Error: B FIFO should be empty after reading response */
		ret = ERROR_FAIL;

		count = 0;
		while (REG32_GET_FIELD(reg, AXI_LSCU__STATUS, B_FIFO_EMPTY) == 0 &&
			   count < AXI_FIFO_DEPTH_B) {
			reg = 0;
			REG32_SET_FIELD(&reg, AXI_LSCU__CMD, REQ, AXI_LSCU__CMD__REQ__VALUE__RLSE_B);
			(void)axi_lscu_reg_write(tap, AXI_LSCU__CMD__ADDR, reg);
			reg = 0;
			(void)axi_lscu_reg_read(tap, AXI_LSCU__STATUS__ADDR, &reg);

			count++;
		}
	}

	return ret;
}
