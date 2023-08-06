set dir [file dirname [info script]]

package ifneeded awsdynamodb 0.1 [list load [file join $dir libaws-sdk-tcl-dynamodb.so] Aws_sdk_tcl_dynamodb]
