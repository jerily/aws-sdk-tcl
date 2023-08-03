set dir [file dirname [dict get [info frame 0] file]]
load [file join $dir build libaws-sdk-tcl-s3.so] Aws_sdk_tcl_s3
puts [::aws::s3::ls some_handle some-bucket]