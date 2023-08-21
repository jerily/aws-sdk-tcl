set dir [file dirname [info script]]

package ifneeded awssqs 0.1 [list load [file join $dir libaws-sdk-tcl-sqs.so] Aws_sdk_tcl_sqs]
