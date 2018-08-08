/*
 * JTAG to VPI driver
 *
 * Copyright (C) 2013 Franck Jullien, <elec4fun@gmail.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <jtag/interface.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#define NO_TAP_SHIFT	0
#define TAP_SHIFT	1

#define SERVER_ADDRESS	"127.0.0.1"
#define SERVER_PORT	5555

#define	XFERT_MAX_SIZE		512

#define CMD_RESET		0
#define CMD_TMS_SEQ		1
#define CMD_SCAN_CHAIN		2
#define CMD_SCAN_CHAIN_FLIP_TMS	3
#define CMD_STOP_SIMU		4

int server_port = SERVER_PORT;
char *server_address;

int sockfd;
struct sockaddr_in serv_addr;

struct vpi_cmd {
	int cmd;
	unsigned char buffer_out[XFERT_MAX_SIZE];
	unsigned char buffer_in[XFERT_MAX_SIZE];
	int length;
	int nb_bits;
};

static int jtag_vpi_send_cmd(struct vpi_cmd *vpi)
{
	int retval = write_socket(sockfd, vpi, sizeof(struct vpi_cmd));
	if (retval <= 0)
		return ERROR_FAIL;

	return ERROR_OK;
}

static int jtag_vpi_receive_cmd(struct vpi_cmd *vpi)
{
	int retval = read_socket(sockfd, vpi, sizeof(struct vpi_cmd));
	if (retval < (int)sizeof(struct vpi_cmd))
		return ERROR_FAIL;

	return ERROR_OK;
}

/**
 * jtag_vpi_reset - ask to reset the JTAG device
 * @trst: 1 if TRST is to be asserted
 * @srst: 1 if SRST is to be asserted
 */
static int jtag_vpi_reset(int trst, int srst)
{
	struct vpi_cmd vpi;

	vpi.cmd = CMD_RESET;
	vpi.length = 0;
	return jtag_vpi_send_cmd(&vpi);
}

/**
 * jtag_vpi_tms_seq - ask a TMS sequence transition to JTAG
 * @bits: TMS bits to be written (bit0, bit1 .. bitN)
 * @nb_bits: number of TMS bits (between 1 and 8)
 *
 * Write a serie of TMS transitions, where each transition consists in :
 *  - writing out TCK=0, TMS=<new_state>, TDI=<???>
 *  - writing out TCK=1, TMS=<new_state>, TDI=<???> which triggers the transition
 * The function ensures that at the end of the sequence, the clock (TCK) is put
 * low.
 */
static int jtag_vpi_tms_seq(const uint8_t *bits, int nb_bits)
{
	struct vpi_cmd vpi;
	int nb_bytes;

	nb_bytes = DIV_ROUND_UP(nb_bits, 8);

	vpi.cmd = CMD_TMS_SEQ;
	memcpy(vpi.buffer_out, bits, nb_bytes);
	vpi.length = nb_bytes;
	vpi.nb_bits = nb_bits;

	return jtag_vpi_send_cmd(&vpi);
}

/**
 * jtag_vpi_path_move - ask a TMS sequence transition to JTAG
 * @cmd: path transition
 *
 * Write a serie of TMS transitions, where each transition consists in :
 *  - writing out TCK=0, TMS=<new_state>, TDI=<???>
 *  - writing out TCK=1, TMS=<new_state>, TDI=<???> which triggers the transition
 * The function ensures that at the end of the sequence, the clock (TCK) is put
 * low.
 */

static int jtag_vpi_path_move(struct pathmove_command *cmd)
{
	uint8_t trans[DIV_ROUND_UP(cmd->num_states, 8)];

	memset(trans, 0, DIV_ROUND_UP(cmd->num_states, 8));

	for (int i = 0; i < cmd->num_states; i++) {
		if (tap_state_transition(tap_get_state(), true) == cmd->path[i])
			buf_set_u32(trans, i, 1, 1);
		tap_set_state(cmd->path[i]);
	}

	return jtag_vpi_tms_seq(trans, cmd->num_states);
}

/**
 * jtag_vpi_tms - ask a tms command
 * @cmd: tms command
 */
static int jtag_vpi_tms(struct tms_command *cmd)
{
	return jtag_vpi_tms_seq(cmd->bits, cmd->num_bits);
}

static int jtag_vpi_state_move(tap_state_t state)
{
	if (tap_get_state() == state)
		return ERROR_OK;

	uint8_t tms_scan = tap_get_tms_path(tap_get_state(), state);
	int tms_len = tap_get_tms_path_len(tap_get_state(), state);

	int retval = jtag_vpi_tms_seq(&tms_scan, tms_len);
	if (retval != ERROR_OK)
		return retval;

	tap_set_state(state);

	return ERROR_OK;
}

static int jtag_vpi_queue_tdi_xfer(uint8_t *bits, int nb_bits, int tap_shift)
{
	struct vpi_cmd vpi;
	int nb_bytes = DIV_ROUND_UP(nb_bits, 8);

	vpi.cmd = tap_shift ? CMD_SCAN_CHAIN_FLIP_TMS : CMD_SCAN_CHAIN;

	if (bits)
		memcpy(vpi.buffer_out, bits, nb_bytes);
	else
		memset(vpi.buffer_out, 0xff, nb_bytes);

	vpi.length = nb_bytes;
	vpi.nb_bits = nb_bits;

	int retval = jtag_vpi_send_cmd(&vpi);
	if (retval != ERROR_OK)
		return retval;

	retval = jtag_vpi_receive_cmd(&vpi);
	if (retval != ERROR_OK)
		return retval;

	if (bits)
		memcpy(bits, vpi.buffer_in, nb_bytes);

	return ERROR_OK;
}

/**
 * jtag_vpi_queue_tdi - short description
 * @bits: bits to be queued on TDI (or NULL if 0 are to be queued)
 * @nb_bits: number of bits
 */
static int jtag_vpi_queue_tdi(uint8_t *bits, int nb_bits, int tap_shift)
{
	int nb_xfer = DIV_ROUND_UP(nb_bits, XFERT_MAX_SIZE * 8);
	int retval;

	while (nb_xfer) {
		if (nb_xfer ==  1) {
			retval = jtag_vpi_queue_tdi_xfer(bits, nb_bits, tap_shift);
			if (retval != ERROR_OK)
				return retval;
		} else {
			retval = jtag_vpi_queue_tdi_xfer(bits, XFERT_MAX_SIZE * 8, NO_TAP_SHIFT);
			if (retval != ERROR_OK)
				return retval;
			nb_bits -= XFERT_MAX_SIZE * 8;
			if (bits)
				bits += XFERT_MAX_SIZE;
		}

		nb_xfer--;
	}

	return ERROR_OK;
}

/**
 * jtag_vpi_clock_tms - clock a TMS transition
 * @tms: the TMS to be sent
 *
 * Triggers a TMS transition (ie. one JTAG TAP state move).
 */
static int jtag_vpi_clock_tms(int tms)
{
	const uint8_t tms_0 = 0;
	const uint8_t tms_1 = 1;

	return jtag_vpi_tms_seq(tms ? &tms_1 : &tms_0, 1);
}

/**
 * jtag_vpi_scan - launches a DR-scan or IR-scan
 * @cmd: the command to launch
 *
 * Launch a JTAG IR-scan or DR-scan
 *
 * Returns ERROR_OK if OK, ERROR_xxx if a read/write error occured.
 */
static int jtag_vpi_scan(struct scan_command *cmd)
{
	int scan_bits;
	uint8_t *buf = NULL;
	int retval = ERROR_OK;

	scan_bits = jtag_build_buffer(cmd, &buf);

	if (cmd->ir_scan) {
		retval = jtag_vpi_state_move(TAP_IRSHIFT);
		if (retval != ERROR_OK)
			return retval;
	} else {
		retval = jtag_vpi_state_move(TAP_DRSHIFT);
		if (retval != ERROR_OK)
			return retval;
	}

	if (cmd->end_state == TAP_DRSHIFT) {
		retval = jtag_vpi_queue_tdi(buf, scan_bits, NO_TAP_SHIFT);
		if (retval != ERROR_OK)
			return retval;
	} else {
		retval = jtag_vpi_queue_tdi(buf, scan_bits, TAP_SHIFT);
		if (retval != ERROR_OK)
			return retval;
	}

	if (cmd->end_state != TAP_DRSHIFT) {
		/*
		 * As our JTAG is in an unstable state (IREXIT1 or DREXIT1), move it
		 * forward to a stable IRPAUSE or DRPAUSE.
		 */
		retval = jtag_vpi_clock_tms(0);
		if (retval != ERROR_OK)
			return retval;

		if (cmd->ir_scan)
			tap_set_state(TAP_IRPAUSE);
		else
			tap_set_state(TAP_DRPAUSE);
	}

	retval = jtag_read_buffer(buf, cmd);
	if (retval != ERROR_OK)
		return retval;

	if (buf)
		free(buf);

	if (cmd->end_state != TAP_DRSHIFT) {
		retval = jtag_vpi_state_move(cmd->end_state);
		if (retval != ERROR_OK)
			return retval;
	}

	return ERROR_OK;
}

static int jtag_vpi_runtest(int cycles, tap_state_t state)
{
	int retval;

	retval = jtag_vpi_state_move(TAP_IDLE);
	if (retval != ERROR_OK)
		return retval;

	retval = jtag_vpi_queue_tdi(NULL, cycles, TAP_SHIFT);
	if (retval != ERROR_OK)
		return retval;

	return jtag_vpi_state_move(state);
}

static int jtag_vpi_stableclocks(int cycles)
{
	return jtag_vpi_queue_tdi(NULL, cycles, TAP_SHIFT);
}

static int jtag_vpi_execute_queue(void)
{
	struct jtag_command *cmd;
	int retval = ERROR_OK;

	for (cmd = jtag_command_queue; retval == ERROR_OK && cmd != NULL;
	     cmd = cmd->next) {
		switch (cmd->type) {
		case JTAG_RESET:
			retval = jtag_vpi_reset(cmd->cmd.reset->trst, cmd->cmd.reset->srst);
			break;
		case JTAG_RUNTEST:
			retval = jtag_vpi_runtest(cmd->cmd.runtest->num_cycles,
						  cmd->cmd.runtest->end_state);
			break;
		case JTAG_STABLECLOCKS:
			retval = jtag_vpi_stableclocks(cmd->cmd.stableclocks->num_cycles);
			break;
		case JTAG_TLR_RESET:
			retval = jtag_vpi_state_move(cmd->cmd.statemove->end_state);
			break;
		case JTAG_PATHMOVE:
			retval = jtag_vpi_path_move(cmd->cmd.pathmove);
			break;
		case JTAG_TMS:
			retval = jtag_vpi_tms(cmd->cmd.tms);
			break;
		case JTAG_SLEEP:
			jtag_sleep(cmd->cmd.sleep->us);
			break;
		case JTAG_SCAN:
			retval = jtag_vpi_scan(cmd->cmd.scan);
			break;
		}
	}

	return retval;
}

static int jtag_vpi_init(void)
{
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		LOG_ERROR("Could not create socket");
		return ERROR_FAIL;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(server_port);

	if (!server_address)
		server_address = strdup(SERVER_ADDRESS);

	serv_addr.sin_addr.s_addr = inet_addr(server_address);

	if (serv_addr.sin_addr.s_addr == INADDR_NONE) {
		LOG_ERROR("inet_addr error occured");
		return ERROR_FAIL;
	}

	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		close(sockfd);
		LOG_ERROR("Can't connect to %s : %u", server_address, server_port);
		return ERROR_COMMAND_CLOSE_CONNECTION;
	}

	LOG_INFO("Connection to %s : %u succeed", server_address, server_port);

	return ERROR_OK;
}

static int jtag_vpi_quit(void)
{
	free(server_address);
	return close(sockfd);
}

COMMAND_HANDLER(jtag_vpi_set_port)
{
	if (CMD_ARGC == 0)
		LOG_WARNING("You need to set a port number");
	else
		COMMAND_PARSE_NUMBER(int, CMD_ARGV[0], server_port);

	LOG_INFO("Set server port to %u", server_port);

	return ERROR_OK;
}

COMMAND_HANDLER(jtag_vpi_set_address)
{
	free(server_address);

	if (CMD_ARGC == 0) {
		LOG_WARNING("You need to set an address");
		server_address = strdup(SERVER_ADDRESS);
	} else
		server_address = strdup(CMD_ARGV[0]);

	LOG_INFO("Set server address to %s", server_address);

	return ERROR_OK;
}

static const struct command_registration jtag_vpi_command_handlers[] = {
	{
		.name = "jtag_vpi_set_port",
		.handler = &jtag_vpi_set_port,
		.mode = COMMAND_CONFIG,
		.help = "set the port of the VPI server",
		.usage = "description_string",
	},
	{
		.name = "jtag_vpi_set_address",
		.handler = &jtag_vpi_set_address,
		.mode = COMMAND_CONFIG,
		.help = "set the address of the VPI server",
		.usage = "description_string",
	},
	COMMAND_REGISTRATION_DONE
};

struct jtag_interface jtag_vpi_interface = {
	.name = "jtag_vpi",
	.supported = DEBUG_CAP_TMS_SEQ,
	.commands = jtag_vpi_command_handlers,
	.transports = jtag_only,

	.init = jtag_vpi_init,
	.quit = jtag_vpi_quit,
	.execute_queue = jtag_vpi_execute_queue,
};
