package require awss3

set dir [file dirname [dict get [info frame 0] file]]

set bucket_name "my-bucket"

set config_dict [dict create aws_access_key_id "" aws_secret_access_key "" region "us-east-1"]
set s3_client [::aws::s3::create $config_dict]


set lst [::aws::s3::ls $s3_client $bucket_name "test.txt"]
puts lst=$lst

#::aws::s3::destroy $s3_client
$s3_client destroy