proc run_commands { commands_with_args } {
    foreach {cmd args} $commands_with_args {
        echo [$cmd {*}[split $args " "]]
    }
}

if {[catch {run_commands $commands} result]} {
    echo "ERROR: $result"
    shutdown error
}