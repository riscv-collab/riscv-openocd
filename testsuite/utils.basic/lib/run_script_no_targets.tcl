if {[catch {targets 0} result]} {
    echo "SUCCESS: targets 0 command successfully failed"
    shutdown
}
shutdown error
