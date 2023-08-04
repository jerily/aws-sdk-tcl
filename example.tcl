set dir [file dirname [dict get [info frame 0] file]]
load [file join $dir build libaws-sdk-tcl-s3.so] Aws_sdk_tcl_s3

set bucket_name "my-bucket"

set config_dict [dict create]
set s3_client [::aws::s3::create config_dict]
::aws::s3::put $s3_client $bucket_name "test.txt" "Hello World"
set lst [::aws::s3::ls $s3_client $bucket_name]
puts lst=$lst