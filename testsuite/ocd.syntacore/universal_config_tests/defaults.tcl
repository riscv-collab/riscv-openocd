init

if { $NCORES == 1 } {
  if { [string first riscv.cpu [target names]] != 0 } {
    echo "ERROR: unexpected chipname <$names>!"
    shutdown error
  }
} else {
  set idx 0
  foreach name [target names] {
    if { [string first riscv.cpu$idx $name] != 0 } {
      echo "ERROR: unexpected chipname <$name>!"
      shutdown error
    }
    incr idx
  }
}

set idx 0
foreach name [target names] {
  echo $name
  set AREA_SIZE 0x10000
  set AREA_BASE [expr {0x3ff0000 - $idx * $AREA_SIZE}]
  set AREA_BACKUP 1

  set wa_base [$name cget -work-area-phys]
  set wa_size [$name cget -work-area-size]
  set wa_backup [$name cget -work-area-backup]

  if { $wa_base != $AREA_BASE } {
    echo "ERROR: work-area-phys is $wa_base ($AREA_BASE expected)"
    shutdown error
  }

  if { $wa_size != $AREA_SIZE } {
    echo "ERROR: work-area-phys is $wa_size ($AREA_SIZE expected)"
    shutdown error
  }

  if { [ $name cget -work-area-backup ] != $AREA_BACKUP } {
    echo "ERROR: work-area-backup is $wa_backup ($AREA_BACKUP expected)"
    shutdown error
  }

  set EXPECTED_RST "none separate"
  set RST [string trim [reset_config]]
  echo $RST
  if { $RST != $EXPECTED_RST } {
    echo "ERROR: reset_config $RST (while *$EXPECTED_RST* expected)"
    shutdown error
  }
  incr idx
}

shutdown
