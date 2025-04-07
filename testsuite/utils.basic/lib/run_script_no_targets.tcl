if {[catch {targets 0} result]} {
    echo "SUCCESS: targets 0 command successfully failed"
    shutdown
}
echo "[targets]"
echo "FAILURE: for reasons unknown and obscure target presence is detected"
shutdown error
