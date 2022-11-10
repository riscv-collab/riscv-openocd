sc_target_config work-area-enable 0
sc_target_config work-area-phys   0x1000
sc_target_config work-area-size   16
sc_target_config work-area-backup 0

init

foreach name [target names] {

  echo $name

  set base [ $name cget -work-area-phys ]
  set size [ $name cget -work-area-size ]
  set backup [ $name cget -work-area-backup ]

  if { $base != 0 } {
    echo "ERROR: work-area-phys is non-zero ($base)"
    shutdown error
  }
  if { $size != 0 } {
    echo "ERROR: work-area-size is non-zero ($size)"
    shutdown error
  }
  if { $backup != 0 } {
    echo "ERROR: work-area-backup is non-zero ($backup)"
    shutdown error
  }
}

shutdown
