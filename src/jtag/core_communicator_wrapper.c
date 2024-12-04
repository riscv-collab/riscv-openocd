#include "core_communicator_wrapper.h"
#include "jtag.h"
#include "target/riscv/riscv.h"
#include "target/target.h"
#include "transport/transport.h"
#include "drivers/riscv_dtm/dtm.h"


void wrapper_tap_init(struct jtag_tap *tap){
    if (transport_is_jtag()) {
        return jtag_tap_init(tap);
    }
    LOG_INFO("tap_init is only supported for JTAG. It does nothing on other transports.");
}

void wrapper_tap_free(struct jtag_tap *tap){
    if (transport_is_jtag()){
        return jtag_tap_init(tap);
    }
    LOG_INFO("tap_free is only supported for JTAG. It does nothing on other transports.");
}

struct jtag_tap *wrapper_all_taps(void){
    if (transport_is_jtag()){
        return jtag_all_taps();
    }
    LOG_INFO("all_taps is only supported for JTAG. It returns nothing on other transports.");
    return NULL;
}

const char *wrapper_tap_name(const struct jtag_tap *tap){
    if (transport_is_jtag()) {
        return jtag_tap_name(tap);
    }
    LOG_INFO("tap_name is only supported for JTAG. It returns nothing on other transports.");
    return NULL;
}

struct jtag_tap *wrapper_tap_by_string(const char *dotted_name){
    if (transport_is_jtag()){
        return jtag_tap_by_string(dotted_name);
    }
    LOG_INFO("tap_by_string is only supported for JTAG. It returns nothing on other transports.");
    return NULL;
}

struct jtag_tap *wrapper_tap_by_position(unsigned int abs_position){
    if (transport_is_jtag()){
        return jtag_tap_by_position(abs_position);
    }
    LOG_INFO("tap_by_position is only supported for JTAG. It returns nothing on other transports.");
    return NULL;
}

struct jtag_tap *wrapper_tap_next_enabled(struct jtag_tap *p){
    if (transport_is_jtag()){
        return jtag_tap_next_enabled(p);
    }
    LOG_INFO("tap_next_enabled is only supported for JTAG. It returns nothing on other transports.");
    return NULL;
}

unsigned int wrapper_tap_count_enabled(void){
    if (transport_is_jtag()){
        return jtag_tap_count_enabled();
    }
    LOG_INFO("tap_count_enabled is only supported for JTAG. It returns nothing on other transports.");
    return ERROR_OK;
}

int wrapper_register_event_callback(jtag_event_handler_t f, void *x){
    if (transport_is_jtag()){
        return jtag_register_event_callback(f, x);
    }
    LOG_INFO("register_event_callback is only supported for JTAG. It returns nothing on other transports.");
    return ERROR_OK;
}

int wrapper_unregister_event_callback(jtag_event_handler_t f, void *x){
    if (transport_is_jtag()){
        return jtag_unregister_event_callback(f, x);
    }
    LOG_INFO("unregister_event_callback is only supported for JTAG. It returns nothing on other transports.");
    return ERROR_OK;
}

int wrapper_call_event_callbacks(enum jtag_event event){
    if (transport_is_jtag()){
        return jtag_call_event_callbacks(event);
    }
    LOG_INFO("unregister_event_callback is only supported for JTAG. It returns nothing on other transports.");
    return ERROR_OK;
}

enum reset_types wrapper_get_reset_config(void){
    if(transport_is_jtag()){
        return jtag_get_reset_config();
    }
    LOG_INFO("get_reset_config is only supported for JTAG. It returns nothing on other transports.");
    return RESET_CNCT_UNDER_SRST; 
}

void wrapper_set_reset_config(enum reset_types type){
    if (transport_is_jtag()){
        return jtag_set_reset_config(type);
    }
    LOG_INFO("set_reset_config is only supported for JTAG. It returns nothing on other transports.");
}

void wrapper_set_nsrst_delay(unsigned int delay){
    if (transport_is_jtag()){
        return jtag_set_nsrst_delay(delay);
    }
    LOG_INFO("set_nsrst_delay is only supported for JTAG. It returns nothing on other transports.");
}

unsigned int wrapper_get_nsrst_delay(void){
    if (transport_is_jtag()){
        return jtag_get_nsrst_delay();
    }
    LOG_INFO("get_nsrst_delay is only supported for JTAG. It returns nothing on other transports.");
    return ERROR_OK;
}

void wrapper_set_ntrst_delay(unsigned int delay){
    if(transport_is_jtag()){
        return jtag_set_ntrst_delay(delay);
    }
    LOG_INFO("set_ntrst_delay is only supported for JTAG. It returns nothing on other transports.");
}

unsigned int wrapper_get_ntrst_delay(void){
    if(transport_is_jtag()){
        return jtag_get_ntrst_delay();
    }
    LOG_INFO("get_ntrst_delay is only supported for JTAG. It returns nothing on other transports.");
    return ERROR_OK;
}

void wrapper_set_nsrst_assert_width(unsigned int delay){
    if(transport_is_jtag()){
        return jtag_set_nsrst_assert_width(delay);
    }
    LOG_INFO("set_nsrst_assert_width is only supported for JTAG. It returns nothing on other transports.");
}

unsigned int wrapper_get_nsrst_assert_width(void){
    if(transport_is_jtag()){
        return jtag_get_nsrst_assert_width();
    }
    LOG_INFO("get_nsrst_assert_width is only supported for JTAG. It returns nothing on other transports.");
    return ERROR_OK;
}

void wrapper_set_ntrst_assert_width(unsigned int delay){
    if (transport_is_jtag()) {
        return jtag_set_ntrst_assert_width(delay);
    } 
    LOG_INFO("set_nsrst_assert_width is only supported for JTAG. It returns nothing on other transports.");
}

unsigned int wrapper_get_ntrst_assert_width(void){
    if (transport_is_jtag()) {
        return jtag_get_ntrst_assert_width();
    }
    LOG_INFO("set_ntrst_assert_width is only supported for JTAG. It returns nothing on other transports.");
    return ERROR_OK;
}

int wrapper_get_trst(void){
    if (transport_is_jtag()) {
        return jtag_get_trst();
    }
    LOG_INFO("get_trst is only supported for JTAG. It returns nothing on other transports.");
    return ERROR_OK;
}

int wrapper_get_srst(void){
    if (transport_is_jtag()) {
        return jtag_get_srst();
    }
    LOG_INFO("get_srst is only supported for JTAG. It returns nothing on other transports.");
    return ERROR_OK;
}

void wrapper_set_verify(bool enable){
     if (transport_is_jtag()) {
        jtag_set_verify(enable);
    }
    LOG_INFO("set_verify is only supported for JTAG. It returns nothing on other transports.");
}

bool wrapper_will_verify(void){
    if (transport_is_jtag()) {
        return jtag_will_verify();
    }
    LOG_INFO("will_verify is only supported for JTAG. It returns nothing on other transports.");
    return false;
}

void wrapper_set_verify_capture_ir(bool enable){
    if (transport_is_jtag()) {
        return jtag_set_verify_capture_ir(enable);
    } 
    LOG_INFO("set_verify_capture is only supported for JTAG. It returns nothing on other transports.");
}

bool wrapper_will_verify_capture_ir(void){
    if (transport_is_jtag()) {
        return jtag_will_verify_capture_ir();
    }
    LOG_INFO("will_verify_capture is only supported for JTAG. It returns nothing on other transports.");
    return false;
}

void wrapper_set_flush_queue_sleep(int ms){
    if (transport_is_jtag()) {
        return jtag_set_flush_queue_sleep(ms);
    }
    LOG_INFO("set_flush_queue_sleep is only supported for JTAG. It returns nothing on other transports.");
}

int wrapper_init(struct command_context *cmd_ctx){
    if (transport_is_jtag()) {
        return jtag_init(cmd_ctx);
    } 
    if (transport_is_socket()) {
        // Call the socket initialization function
        return socket_dmi_init(get_active_dtm_driver()); 
    }
    LOG_ERROR("Unsupported transport for init");
    return ERROR_FAIL;
}

int wrapper_init_reset(struct command_context *cmd_ctx){
    if (transport_is_jtag()) {
        return jtag_init_reset(cmd_ctx);
    } else if (transport_is_socket()) {
        dtm_driver_t *active_driver = get_active_dtm_driver();
        if (!active_driver || strcmp(active_driver->name, "socket") != 0) {
            LOG_ERROR("Active DTM driver is not socket");
            return ERROR_FAIL;
        }
        // clear_socket_buffer(active_driver->priv->sock_fd);
        if (socket_dmi_init(active_driver) != 0) {
            LOG_ERROR("Failed to connect to socket.");
            return ERROR_FAIL;
        }
    }
    LOG_ERROR("Unsupported transport for init_reset");
    return ERROR_FAIL;
}

int wrapper_register_commands(struct command_context *cmd_ctx){
    if (transport_is_jtag()) {
        return jtag_register_commands(cmd_ctx);
    } else if (transport_is_socket()) {
        LOG_DEBUG("register_commands called for socket transport.");
        return ERROR_OK;
    } else {
        LOG_ERROR("Unsupported transport for register_commands");
        return ERROR_FAIL;
    }
}

int wrapper_init_inner(struct command_context *cmd_ctx){
    if (transport_is_jtag()) {
        return jtag_init_inner(cmd_ctx);
    } else if (transport_is_socket()) {
        LOG_DEBUG("init_inner: Initializing socket...");
        return socket_dmi_init(get_active_dtm_driver());
    } else {
        LOG_ERROR("Unsupported transport for init_inner");
        return ERROR_FAIL;
    }
}


void wrapper_add_ir_scan(struct jtag_tap *tap,
    struct scan_field *fields, enum tap_state endstate){

    if (transport_is_jtag()) {
        return jtag_add_ir_scan(tap, fields, endstate);
    } 
    if (transport_is_socket()) {
        LOG_ERROR("JTAG IR scan operations not supported on socket transport");
        return;
    }
    LOG_ERROR("Unsupported transport for add_ir_scan");
}

void wrapper_add_ir_scan_noverify(struct jtag_tap *tap,
    const struct scan_field *fields, enum tap_state state){

    if (transport_is_jtag()) {
        return jtag_add_ir_scan_noverify(tap, fields, state);
    } 
    if (transport_is_socket()) {
        LOG_ERROR("JTAG IR scan operations not supported on socket transport");
        return;
    }
    LOG_ERROR("Unsupported transport for add_ir_scan_noverify");
}

void wrapper_add_plain_ir_scan(int num_bits, const uint8_t *out_bits, uint8_t *in_bits,
		enum tap_state endstate){

    if (transport_is_jtag()) {
        return jtag_add_plain_ir_scan(num_bits, out_bits, in_bits, endstate);
    }
    if (transport_is_socket()) {
        LOG_ERROR("JTAG IR scan operations not supported on socket transport");
        return;
    } 
    LOG_ERROR("Unsupported transport for add_plain_ir_scan");
}

void wrapper_add_dr_scan(struct jtag_tap *tap, int num_fields,
		const struct scan_field *fields, enum tap_state endstate){

    if (transport_is_jtag()) {
        return jtag_add_dr_scan(tap, num_fields, fields, endstate);
    } 
    if (transport_is_socket()) {
        LOG_ERROR("JTAG DR scan operations not supported on socket transport");
        return;
    }
    LOG_ERROR("Unsupported transport for add_dr_scan");
}

void wrapper_add_dr_scan_check(struct jtag_tap *tap, int num_fields,
		struct scan_field *fields, enum tap_state endstate){

    if (transport_is_jtag()) {
        return jtag_add_dr_scan_check(tap, num_fields, fields, endstate);
    }
    if (transport_is_socket()) {
        LOG_ERROR("JTAG DR scan operations not supported on socket transport");
        return;
    }
    LOG_ERROR("Unsupported transport for add_dr_scan_check");
}

void wrapper_add_plain_dr_scan(int num_bits,
		const uint8_t *out_bits, uint8_t *in_bits, enum tap_state endstate){

    if (transport_is_jtag()) {
        return jtag_add_plain_dr_scan(num_bits, out_bits, in_bits, endstate);
    }
    if (transport_is_socket()) {
        LOG_ERROR("JTAG DR scan operations not supported on socket transport");
        return;
    }
    LOG_ERROR("Unsupported transport for add_plain_dr_scan");
}

void wrapper_add_tlr(void){
    if (transport_is_jtag()) {
        return jtag_add_tlr();
    }
    if (transport_is_socket()) {
        LOG_ERROR("JTAG Test Logic Reset not directly supported on socket transport");
        // We could potentially implement a DMI equivalent of TLR here
        // if our RISC-V target supports resetting the debug module
        // through a specific DMI command.
        // TO DO: Check and implement if needed
        return;
    }
    LOG_ERROR("Unsupported transport for add_tlr");
}

void wrapper_add_pathmove(unsigned int num_states, const enum tap_state *path){
    if (transport_is_jtag()) {
        return jtag_add_pathmove(num_states, path);
    } 
    if (transport_is_socket()) {
        LOG_ERROR("JTAG pathmove not supported on socket transport");
        return; 
    }
    LOG_ERROR("Unsupported transport for add_pathmove");
}

int wrapper_add_statemove(enum tap_state goal_state){
    if (transport_is_jtag()) {
        return jtag_add_statemove(goal_state);
    } 
    if (transport_is_socket()) {
        LOG_ERROR("JTAG statemove not supported on socket transport");
        return ERROR_FAIL;
    }
    LOG_ERROR("Unsupported transport for add_statemove");
    return ERROR_FAIL;
}

void wrapper_add_runtest(unsigned int num_cycles, enum tap_state endstate){
    if (transport_is_jtag()) {
        return jtag_add_runtest(num_cycles, endstate);
    } 
    if (transport_is_socket()) {
        LOG_ERROR("JTAG runtest not supported on socket transport");
        return;
    }
    LOG_ERROR("Unsupported transport for add_runtest");
}

void wrapper_add_reset(int req_tlr_or_trst, int srst){
    if (transport_is_jtag()) {
        return jtag_add_reset(req_tlr_or_trst, srst);
    } 
    if (transport_is_socket()) {
        LOG_ERROR("JTAG reset not directly supported on socket transport");
        // We might implement a DMI-based reset here, if available. Currently not yet available!
    }
    LOG_ERROR("Unsupported transport for add_reset");
}

void wrapper_add_sleep(uint32_t us){
    if (transport_is_jtag()) {
        return jtag_add_sleep(us);
    } 
    if (transport_is_socket()) {
        // We can use usleep directly or a socket-specific delay, if needed.
        usleep(us);
        return;
    }
    LOG_ERROR("Unsupported transport for add_sleep");
}

int wrapper_add_tms_seq(unsigned int nbits, const uint8_t *seq, enum tap_state t){
    if (transport_is_jtag()) {
        return jtag_add_tms_seq(nbits, seq, t);
    } 
    if (transport_is_socket()) {
        LOG_ERROR("JTAG TMS sequences not supported on socket transport");
        return ERROR_FAIL;
    }
    LOG_ERROR("Unsupported transport for add_tms_seq");
    return ERROR_FAIL;
}

void wrapper_add_clocks(unsigned int num_cycles){
    if (transport_is_jtag()) {
        return jtag_add_clocks(num_cycles);
    } 
    if (transport_is_socket()) {
        LOG_ERROR("JTAG clock operations not supported on socket transport");
        return;
    }
    LOG_ERROR("Unsupported transport for add_clocks");
}

int wrapper_execute_queue(void){
    if (transport_is_jtag()) {
        return jtag_execute_queue();
    } 
    if (transport_is_socket()) {
        // Socket transport doesn't use a JTAG queue
        LOG_ERROR("execute_queue not applicable to socket transport, error or do nothing.");
        return ERROR_OK;
    }
    LOG_ERROR("Unsupported transport for execute_queue");
    return ERROR_FAIL;
}

void wrapper_execute_queue_noclear(void){
    if (transport_is_jtag()) {
        return jtag_execute_queue_noclear();
    } 
    if (transport_is_socket()) {
        // Socket transport doesn't use a JTAG queue
        LOG_ERROR("execute_queue_noclear not applicable to socket transport, error or do nothing.");
        return;
    }
    LOG_ERROR("Unsupported transport for execute_queue_noclear");
}

unsigned int wrapper_get_flush_queue_count(void){
    if (transport_is_jtag()) {
        return jtag_get_flush_queue_count();
    } 
    if (transport_is_socket()) {
        // Socket transport doesn't use a JTAG queue
        LOG_DEBUG("get_flush_queue_count called for socket transport, returning default value (0).");
        return 0;
    }
    LOG_ERROR("Unsupported transport for get_flush_queue_count");
    return 0;
}

int wrapper_power_dropout(int *dropout){
    if (transport_is_jtag()) {
        return jtag_power_dropout(dropout);
    } 
    if (transport_is_socket()) {
        // Socket transport might have its own power detection, otherwise return default
        LOG_DEBUG("jtag_power_dropout called for socket transport, returning default value (0).");
        *dropout = 0;
        return ERROR_OK;
    }
    LOG_ERROR("Unsupported transport for jtag_power_dropout");
    return ERROR_FAIL;
}

int wrapper_srst_asserted(int *srst_asserted){
    if (transport_is_jtag()) {
        return jtag_srst_asserted(srst_asserted);
    } 
    if (transport_is_socket()) {
        // Socket transport might have its own SRST detection, otherwise return default
        LOG_DEBUG("jtag_srst_asserted called for socket transport, returning default value (0).");
        *srst_asserted = 0;
        return ERROR_OK;
    }
    LOG_ERROR("Unsupported transport for jtag_srst_asserted");
    return ERROR_FAIL;
}

void wrapper_check_value_mask(struct scan_field *field, uint8_t *value, uint8_t *mask){
    if (transport_is_jtag()) {
        return jtag_check_value_mask(field, value, mask);
    } 
    if (transport_is_socket()) {
        LOG_ERROR("JTAG value/mask checking not supported on socket transport");
        return;
    }
    LOG_ERROR("Unsupported transport for check_value_mask");
}

void wrapper_sleep(uint32_t us){
    if (transport_is_jtag()) {
        return jtag_sleep(us);
    } 
    if (transport_is_socket()) {
        // Use usleep directly or a socket-specific delay
        usleep(us);
        return;
    }
    LOG_ERROR("Unsupported transport for sleep");
}

void wrapper_set_error(int error){
    if (transport_is_jtag()) {
        return jtag_set_error(error);
    } 
    if (transport_is_socket()) {
        // Socket transport might have its own error handling, otherwise do nothing
        LOG_DEBUG("jtag_set_error called for socket transport, doing nothing.");
        return;
    }
    LOG_ERROR("Unsupported transport for jtag_set_error");
}

bool wrapper_is_jtag_poll_safe(void){
    if (transport_is_jtag()) {
        return is_jtag_poll_safe();
    } 
    if (transport_is_socket()) {
        // Whether polling is safe depends on the socket implementation.
        // We might need to add a mechanism to check if polling is safe
        // based on the state of our socket connection.
        // TODO: This is a future improvement.
        LOG_DEBUG("is_jtag_poll_safe called for socket transport, returning default value (false).");
        return false; 
    }
    LOG_ERROR("Unsupported transport for is_jtag_poll_safe");
    return false;
}

bool wrapper_poll_get_enabled(void){
    if (transport_is_jtag()) {
        return jtag_poll_get_enabled();
    } 
    if (transport_is_socket()) {
        // Socket transport might have its own polling mechanism, otherwise return default
        LOG_DEBUG("jtag_poll_get_enabled called for socket transport, returning default value (false).");
        return false;
    }
    LOG_ERROR("Unsupported transport for jtag_poll_get_enabled");
    return false;
}

void wrapper_poll_set_enabled(bool value){
    if (transport_is_jtag()) {
        return jtag_poll_set_enabled(value);
    } 
    if (transport_is_socket()) {
        // Socket transport might have its own polling mechanism, otherwise do nothing
        LOG_DEBUG("jtag_poll_set_enabled called for socket transport, doing nothing.");
        return;
    }
    LOG_ERROR("Unsupported transport for jtag_poll_set_enabled");
}

bool wrapper_poll_mask(void){
    if (transport_is_jtag()) {
        return jtag_poll_mask();
    } 
    if (transport_is_socket()) {
        // Socket transport might have its own polling mechanism, otherwise return default
        LOG_DEBUG("jtag_poll_mask called for socket transport, returning default value (false).");
        return false;
    }
    LOG_ERROR("Unsupported transport for jtag_poll_mask");
    return false;
}

void wrapper_poll_unmask(bool saved){
    if (transport_is_jtag()) {
        return jtag_poll_unmask(saved);
    } 
    if (transport_is_socket()) {
        // Socket transport might have its own polling mechanism, otherwise do nothing
        LOG_DEBUG("jtag_poll_unmask called for socket transport, doing nothing.");
        return;
    }
    LOG_ERROR("Unsupported transport for jtag_poll_unmask");
}
