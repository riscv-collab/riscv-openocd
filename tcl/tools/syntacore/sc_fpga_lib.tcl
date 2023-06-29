# A collection of reference scripts that are useful for basic fpga tasks
#
# If you experience difficulties with these scripts or would like to introduce
# a new functionality - feel free to contact OpenOCD maintainer at Syntacore.

# Known Problems/Bugs:
#
# - sc_fpga_halt_all/sc_fpga_resume_all are not bullet-proof. The current
#   implementation assumes that only one SMP group can exist and that all
#   targets are part of it.
#   The problem is that it is possible to have several SMP groups or to have a
#   target that does not belong to any. Currently, OpenOCD does not expose
#   functionality to identify which target belongs to which group. It only
#   allows us to query if a target belongs to some group. So these functions
#   are not safe to use in such contexts. That beeing said, we do hope that
#   such contexts are seldom (well, never :), to be more precise) used and most
#   users don't need to worry about that.

namespace eval _SC_INTERNALS {
    proc sc_fpga_fname {} {
        return [lindex [info level -2] 0]
    }

    proc sc_lib_print { msg } {
        # TODO: we could provide an option to silence these messages
        echo "[sc_fpga_fname]: $msg"
    }

    proc fill_gprs_with_zero {} {
        for { set reg_idx 1 } { $reg_idx < 32 } { incr reg_idx } {
            reg $reg_idx 0
        }
    }

    proc sc_lib_do_resume_impl { addr } {
        if { $addr eq ""} {
            resume
        } else {
            resume $addr
        }
    }

    proc sc_lib_do_resume { addr } {
        # TODO: currently, TCL-interpreter used by OpenOCD does not clear
        # pending exception info, so for now we don't use exceptions here
        # set log [capture \
        #    { set code [ catch { sc_fpga_internal_do_resume_impl $addr } ex ] }]
        # if {$code != 0 } {
        #   return -code error "could not resume target! code: $code, log: $log, ex: $ex"
        # }
        sc_lib_do_resume_impl $addr
    }

    proc sc_lib_run_impl { file_path file_type load_address entry_point mode { primary_target ""} } {
        sc_fpga_halt_all
        if { $primary_target eq "" } {
            set primary_target [lindex [target names] 0]
            _SC_INTERNALS::sc_lib_print "$primary_target derived as primary target"
        }
        _SC_INTERNALS::sc_lib_print "switching active target to $primary_target"
        targets $primary_target
        set load_image_args [list $file_path]
        if { $load_address ne "" } {
            lappend load_image_args $load_address
            lappend load_image_args $file_type
        }
        _SC_INTERNALS::sc_lib_print "load_image $load_image_args"
        set load_result [load_image {*}$load_image_args]
        _SC_INTERNALS::sc_lib_print "$load_result"

        if { $entry_point eq "" && $load_address ne "" } {
            _SC_INTERNALS::sc_lib_print \
                "no entry point was specified, using load address ($load_address) as one"
            set entry_point $load_address
        }

        if { $mode eq "all" } {
            sc_fpga_resume_all $entry_point
        } else {
            resume $entry_point
        }
        return $entry_point
    }

    proc apply_for_each_target { function_name args } {
        set current_target [target current]
        foreach t [target names] {
            targets $t
            if {[llength $args] == 0} {
                $function_name
            } else {
                $function_name {*}$args
            }
        }
        targets $current_target
    }
}

proc sc_fpga_halt_all {} {
    # TODO: take into account cases when several SMP groups are present or
    # there are targets that does not belong to any SMP group
    if {[string trim [smp]] eq "off"} {
        _SC_INTERNALS::apply_for_each_target halt
    } else {
        halt
    }
    _SC_INTERNALS::sc_lib_print "all targets halted"
}

proc sc_fpga_resume_all { { addr "" } } {
    # TODO: take into account cases when several SMP groups are present or
    # there are targets that does not belong to any SMP group
    if {[string trim [smp]] eq "off"} {
        _SC_INTERNALS::apply_for_each_target _SC_INTERNALS::sc_lib_do_resume $addr
    } else {
        _SC_INTERNALS::sc_lib_do_resume $addr
    }
    if { $addr eq "" } {
        _SC_INTERNALS::sc_lib_print "all targets resumed"
    } else {
        _SC_INTERNALS::sc_lib_print "all targets resumed at $addr"
    }
}

proc sc_fpga_zero_regs {} {
    _SC_INTERNALS::apply_for_each_target _SC_INTERNALS::fill_gprs_with_zero
    _SC_INTERNALS::sc_lib_print "all general-purpose registers are zero-out"
}

proc sc_fpga_setup_semihosting { {what ""} } {
    # TODO: not the best design... may be changed in future
    riscv set_ebreakm on
    riscv set_ebreaks on
    riscv set_ebreaku on

    _SC_INTERNALS::sc_lib_print "NOTE: dcsr.ebreak{m,s,u} are set to ON"

    if {$what eq "" || $what eq "enable" } {
        return [arm semihosting enable]
    } elseif {$what eq "stop" || $what eq "disable" } {
        return [arm semihosting disable]
    } else {
        return -code error "unknown operation $what passed to semihosting setup"
    }
}

proc sc_fpga_find_target_by_hartid { hartid } {
    set current_target [target current]
    foreach t [target names] {
        targets $t
        if {[catch { reg mhartid } mhartid]} {
            targets $current_target
            return -code error "could not not read mhartid from $t ($mhartid)"
        }
        set hex_value [string trim [lindex [split $mhartid :] 1]]
        set decimal_val [expr $hex_value]
        if { $decimal_val == $hartid } {
            _SC_INTERNALS::sc_lib_print "$t has mhartid of $hartid"
            targets $current_target
            return $t
        }
    }
    targets $current_target
    return -code error "could not find target with mhartid = $hartid"
}

proc sc_fpga_run_elf { file_path entry_point { tgt ""} } {
    set entry_point \
        [_SC_INTERNALS::sc_lib_run_impl $file_path "" "" $entry_point one $tgt]
    _SC_INTERNALS::sc_lib_print \
        "started execution of $file_path with $entry_point as entry point"
}

proc sc_fpga_run_elf_smp { file_path entry_point { tgt ""} } {
    set entry_point \
        [_SC_INTERNALS::sc_lib_run_impl $file_path "" "" $entry_point all $tgt]
    _SC_INTERNALS::sc_lib_print \
        "started execution of $file_path with $entry_point as entry point"
}

proc sc_fpga_run_bin { file_path load_address {entry_point ""} { tgt ""} } {
    set entry_point \
        [_SC_INTERNALS::sc_lib_run_impl $file_path bin $load_address $entry_point one $tgt]
    _SC_INTERNALS::sc_lib_print \
        "started execution of $file_path with $entry_point as entry point"
}

proc sc_fpga_run_bin_smp { file_path load_address {entry_point ""} { tgt ""} } {
    set entry_point \
        [_SC_INTERNALS::sc_lib_run_impl $file_path bin $load_address $entry_point all $tgt]
    _SC_INTERNALS::sc_lib_print \
        "started execution of $file_path with $entry_point as entry point"
}

proc sc_fpga_info {} {
    return [join \
        [list \
            "Version: [version]" \
            "Number of harts: [llength [target names]]" \
            "SMP status: [string trim [smp]]" \
        ] "\n"]
}

echo "--- LOADED SC FPGA LIBRARY ---"
