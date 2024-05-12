set WA_BASE   0x1000
set WA_BACKUP 1

sc_target_config work-area-enable 1
sc_target_config work-area-phys   $WA_BASE
sc_target_config work-area-backup $WA_BACKUP

init

shutdown
