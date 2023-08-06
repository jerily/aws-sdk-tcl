set dir [file dirname [info script]]

package ifneeded awsiam 0.1 [list load [file join $dir libaws-sdk-tcl-iam.so] Aws_sdk_tcl_iam]
