set dir [file dirname [info script]]

package ifneeded awslambda 0.1 [list load [file join $dir libaws-sdk-tcl-lambda.so] Aws_sdk_tcl_lambda]
