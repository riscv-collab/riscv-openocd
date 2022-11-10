set WA_BASE   0x1000
set WA_SIZE   16
set WA_BACKUP 0

sc_target_config work-area-enable 1
sc_target_config work-area-phys   $WA_BASE
sc_target_config work-area-size   $WA_SIZE
sc_target_config work-area-backup $WA_BACKUP

init

set indx 0
foreach name [target names] {
  echo $name

  set expected_wa_base [expr {$WA_BASE - ($WA_SIZE * $indx) }]

  set wa_base [$name cget -work-area-phys]
  set wa_size [ $name cget -work-area-size ]
  set wa_backup [ $name cget -work-area-backup ]

  if { $wa_base != $expected_wa_base } {
    echo "ERROR: work-area-phys is $wa_base ($expected_wa_base expected)"
    shutdown error
  }

  if { $wa_size != $WA_SIZE } {
    echo "ERROR: work-area-phys is $wa_size ($WA_SIZE expected)"
    shutdown error
  }

  if { [ $name cget -work-area-backup ] != $WA_BACKUP } {
    echo "ERROR: work-area-backup is $wa_backup ($WA_BACKUP expected)"
    shutdown error
  }

  incr indx
}

shutdown
