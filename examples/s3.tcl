package require awss3

set dir [file dirname [dict get [info frame 0] file]]
#load [file join $dir .. build src/aws-sdk-tcl-s3 libaws-sdk-tcl-s3.so] Aws_sdk_tcl_s3

set bucket_name "my-bucket"

set config_dict [dict create endpoint "http://s3.localhost.localstack.cloud:4566"]
#set config_dict [dict create]
::aws::s3::create $config_dict s3_client

puts exists_bucket_before=[$s3_client exists_bucket $bucket_name]
$s3_client create_bucket $bucket_name
puts exists_bucket_after=[$s3_client exists_bucket $bucket_name]

puts exists_object_before=[$s3_client exists $bucket_name "test.txt"]
#::aws::s3::put_text $s3_client $bucket_name "test.txt" "Hello World"
$s3_client put_text $bucket_name "test.txt" "Hello World"
puts exists_object_after=[$s3_client exists $bucket_name "test.txt"]

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

$s3_client put_text $bucket_name "test1.txt" "Hello World 1"
$s3_client put_text $bucket_name "test2.txt" "Hello World 2"
puts before_batch_delete=[$s3_client ls $bucket_name]
$s3_client batch_delete $bucket_name [list "test1.txt" "test2.txt"]
puts after_batch_delete=[$s3_client ls $bucket_name]

$s3_client delete_bucket $bucket_name

for {set i 0} {$i < 3} {incr i} {
    set bucket_name "my-bucket-$i"
    $s3_client create_bucket $bucket_name
}
puts buckets_before_delete=[$s3_client list_buckets]
for {set i 0} {$i < 3} {incr i} {
    set bucket_name "my-bucket-$i"
    $s3_client delete_bucket $bucket_name
}
puts buckets_after_delete=[$s3_client list_buckets]


# ::aws::s3::destroy $s3_client
# $s3_client destroy