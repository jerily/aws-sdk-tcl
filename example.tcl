set dir [file dirname [dict get [info frame 0] file]]
load [file join $dir build libaws-sdk-tcl-s3.so] Aws_sdk_tcl_s3

set bucket_name "my-bucket"

set config_dict [dict create]
set s3_client [::aws::s3::create config_dict]
::aws::s3::put $s3_client $bucket_name "test.txt" "Hello World"

#set lst [::aws::s3::ls $s3_client $bucket_name]
#puts lst=$lst
set lst [::aws::s3::ls $s3_client $bucket_name "text.txt"]
puts lst_before_delete=$lst

set text [::aws::s3::get $s3_client $bucket_name "test.txt"]
puts text=$text

set chan [open "myfile.txt" "w"]
::aws::s3::get $s3_client $bucket_name "test.txt" $chan
close $chan
puts files=[glob myfile.*]

::aws::s3::delete $s3_client $bucket_name "test.txt"

set lst [::aws::s3::ls $s3_client $bucket_name "text.txt"]
puts lst_after_delete=$lst
