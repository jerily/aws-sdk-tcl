set dir [file dirname [dict get [info frame 0] file]]
load [file join $dir .. build src/aws-sdk-tcl-s3 libaws-sdk-tcl-s3.so] Aws_sdk_tcl_s3

set bucket_name "my-bucket"

set config_dict [dict create profile localstack region us-east-1 endpoint "http://localhost:4566"]
set s3_client [::aws::s3::create $config_dict]

#::aws::s3::put_text $s3_client $bucket_name "test.txt" "Hello World"
$s3_client put_text $bucket_name "test.txt" "Hello World"

#set lst [::aws::s3::ls $s3_client $bucket_name]
#puts lst=$lst

set lst [::aws::s3::ls $s3_client $bucket_name "test.txt"]
puts lst_before_delete=$lst

set lst [$s3_client ls $bucket_name "test.txt"]
puts lst_with_obj_cmd=$lst

#set text [::aws::s3::get $s3_client $bucket_name "test.txt"]
set text [$s3_client get $bucket_name "test.txt"]
puts text=$text

#set chan [open "myfile.txt" "w"]
#::aws::s3::get $s3_client $bucket_name "test.txt" $chan
#close $chan
$s3_client get $bucket_name "test.txt" myfile.txt
puts files=[glob myfile.*]

::aws::s3::delete $s3_client $bucket_name "test.txt"

set lst [::aws::s3::ls $s3_client $bucket_name "text.txt"]
puts lst_after_delete=$lst

#set from_chan [open [file join $dir .. "Google_2015_logo.png"] "rb"]
#::aws::s3::put $s3_client $bucket_name "my_logo.png" $from_chan
#close $from_chan
::aws::s3::put $s3_client $bucket_name "my_logo.png" [file join $dir .. "Google_2015_logo.png"]
#set to_chan [open "mylogo.png" "wb"]
#::aws::s3::get $s3_client $bucket_name "my_logo.png" $to_chan
#close $to_chan
::aws::s3::get $s3_client $bucket_name "my_logo.png" mylogo.png
::aws::s3::delete $s3_client $bucket_name "my_logo.png"

#::aws::s3::destroy $s3_client
$s3_client destroy