# SPDX-License-Identifier: GPL-2.0-or-later

namespace eval jtag_dummy_testing {

	variable ntaps

	proc setup {arg_ntaps} {
		variable ntaps $arg_ntaps
		adapter driver dummy
		proc ::jtag_init {} {}
		for {set i 0} {$i < $ntaps} {incr i} {
			jtag newtap "tap$i" tap -irlen 1
		}
		init
	}
	namespace export setup

	proc test_failure message {
		echo $message
		shutdown error
	}

	proc check_for_error {expected_code script} {
		set code [catch {uplevel 1 $script} msg]
		if {$code != $expected_code} {
			test_failure \
				"Expecting error code $expected_code, not $code for '$script'. Error message: '$msg'"
		}
	}

	proc check_invalid_arg script {
		tailcall check_for_error -603 $script
	}

	proc check_syntax_err script {
		tailcall check_for_error -601 $script
	}

	proc check_overflow_err script {
		tailcall check_for_error -604 $script
	}

	proc check_underflow_err script {
		tailcall check_for_error -605 $script
	}

	proc check_matches {pattern script} {
		set result [uplevel $script]
		if {[regexp $pattern $result]} {return}
		test_failure \
			"Expecting the result of '$script', which is '$result', to match '$pattern'"
	}
	namespace export check_invalid_arg check_syntax_err check_overflow_err \
		check_underflow_err check_matches
}
