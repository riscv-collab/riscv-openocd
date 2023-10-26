# NO real checks here (we are unable to query this),
# just making sure that we don't crash
sc_target_config gdb_report_data_abort 0
sc_target_config gdb_report_register_access_error 0

sc_target_config expose_csrs 928=some_register,1952-1953,2048

sc_target_config adapter_speed 1

init

shutdown
