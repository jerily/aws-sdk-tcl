set dir [file dirname [info script]]

package ifneeded awss3 0.1 [list load [file join $dir libaws-sdk-tcl-s3.so] Aws_sdk_tcl_s3]
