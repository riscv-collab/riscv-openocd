set WA_SIZE   16
set WA_BACKUP 0

sc_target_config work-area-enable 1
sc_target_config work-area-size   $WA_SIZE
sc_target_config work-area-backup $WA_BACKUP

init

shutdown
