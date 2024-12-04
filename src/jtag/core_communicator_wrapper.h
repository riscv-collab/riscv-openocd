
#ifndef CORE_COMMUNICATOR_WRAPPER_H
#define CORE_COMMUNICATOR_WRAPPER_H

#include "jtag.h"
#include "riscv_socket_dmi.h"
/**
 * @brief This file is used to wrap the JTAG, Socket, etc. core functionalities (core.c implementation) for communicating 
 * with the device as well as batching and executing the instruction commands towards target, but in different ways, 
 * not just JTAG, as it was status quo for many years. Now it can be extended by socket, pcie and any other communication
 * interface, between OpenOCD and hardware/simulator/emulator. 
 */


/** Even though these (below functions) are related to jtag, there are some places where they are used. 
 * So in this way we are consistent in having a wrapper for all possible JTAG and non-JTAG functions
 * that might be used in the code. The functions are still called "wrapper_*" because non-jtag interfaces 
 * will have empty implementations and would return default structs. The reason for this is because 
 * JTAG is deeply embedded into OpenOCD and called in many places, that the cleanest way to use 
 * something else is to replace directly those JTAG calls with wrappers, in the rest of the OpenOCD code. 
*/
void wrapper_tap_init(struct jtag_tap *tap);
void wrapper_tap_free(struct jtag_tap *tap);

struct jtag_tap *wrapper_all_taps(void);
const char *wrapper_tap_name(const struct jtag_tap *tap);
struct jtag_tap *wrapper_tap_by_string(const char *dotted_name);
struct jtag_tap *wrapper_tap_by_position(unsigned int abs_position);
struct jtag_tap *wrapper_tap_next_enabled(struct jtag_tap *p);
unsigned int wrapper_tap_count_enabled(void);

int wrapper_register_event_callback(jtag_event_handler_t f, void *x);
int wrapper_unregister_event_callback(jtag_event_handler_t f, void *x);

int wrapper_call_event_callbacks(enum jtag_event event);

enum reset_types wrapper_get_reset_config(void);
void wrapper_set_reset_config(enum reset_types type);

void wrapper_set_nsrst_delay(unsigned int delay);
unsigned int wrapper_get_nsrst_delay(void);

void wrapper_set_ntrst_delay(unsigned int delay);
unsigned int wrapper_get_ntrst_delay(void);

void wrapper_set_nsrst_assert_width(unsigned int delay);
unsigned int wrapper_get_nsrst_assert_width(void);

void wrapper_set_ntrst_assert_width(unsigned int delay);
unsigned int wrapper_get_ntrst_assert_width(void);

int wrapper_get_trst(void);
int wrapper_get_srst(void);

void wrapper_set_verify(bool enable);
bool wrapper_will_verify(void);

void wrapper_set_verify_capture_ir(bool enable);
bool wrapper_will_verify_capture_ir(void);

void wrapper_set_flush_queue_sleep(int ms);

int wrapper_init(struct command_context *cmd_ctx);

int wrapper_init_reset(struct command_context *cmd_ctx);
int wrapper_register_commands(struct command_context *cmd_ctx);
int wrapper_init_inner(struct command_context *cmd_ctx);


void wrapper_add_ir_scan(struct jtag_tap *tap,
		struct scan_field *fields, enum tap_state endstate);
void wrapper_add_ir_scan_noverify(struct jtag_tap *tap,
		const struct scan_field *fields, enum tap_state state);
void wrapper_add_plain_ir_scan(int num_bits, const uint8_t *out_bits, uint8_t *in_bits,
		enum tap_state endstate);

void wrapper_add_dr_scan(struct jtag_tap *tap, int num_fields,
		const struct scan_field *fields, enum tap_state endstate);
void wrapper_add_dr_scan_check(struct jtag_tap *tap, int num_fields,
		struct scan_field *fields, enum tap_state endstate);
void wrapper_add_plain_dr_scan(int num_bits,
		const uint8_t *out_bits, uint8_t *in_bits, enum tap_state endstate);

void wrapper_add_tlr(void);

void wrapper_add_pathmove(unsigned int num_states, const enum tap_state *path);

int wrapper_add_statemove(enum tap_state goal_state);

void wrapper_add_runtest(unsigned int num_cycles, enum tap_state endstate);

void wrapper_add_reset(int req_tlr_or_trst, int srst);

void wrapper_add_sleep(uint32_t us);

int wrapper_add_tms_seq(unsigned int nbits, const uint8_t *seq, enum tap_state t);

void wrapper_add_clocks(unsigned int num_cycles);

int wrapper_execute_queue(void);

void wrapper_execute_queue_noclear(void);

unsigned int wrapper_get_flush_queue_count(void);

int wrapper_power_dropout(int *dropout);

int wrapper_srst_asserted(int *srst_asserted);

void wrapper_check_value_mask(struct scan_field *field, uint8_t *value, uint8_t *mask);

void wrapper_sleep(uint32_t us);

void wrapper_set_error(int error);

bool wrapper_is_jtag_poll_safe(void);

bool wrapper_poll_get_enabled(void);

void wrapper_poll_set_enabled(bool value);

bool wrapper_poll_mask(void);

void wrapper_poll_unmask(bool saved);

#endif // CORE_COMMUNICATOR_WRAPPER 