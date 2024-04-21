package require awss3

set dir [file dirname [info script]]

set bucket_name "my-bucket"

# To use it with real AWS S3, you can use the following configuration:
# set config_dict [dict create region "us-east-1" aws_access_key_id "your_access_key_id" aws_secret_access_key "your_secret_access_key"]

# To use it with localstack, you can use the following configuration:
set config_dict [dict create endpoint "http://s3.localhost.localstack.cloud:4566"]

# creates an S3 client
::aws::s3::create $config_dict s3_client

# checks if the bucket exists already
set exists_p [$s3_client exists_bucket $bucket_name]

# creates the bucket if it does not exist
if {!$exists_p} {
    $s3_client create_bucket $bucket_name
}

# uploads a file to the bucket with the name "my_logo.png"
$s3_client put $bucket_name "my_logo.png" [file join $dir "Google_2015_logo.png"]

# downloads the file from the bucket and saves it to a file named "mylogo.png"
$s3_client get $bucket_name "my_logo.png" /tmp/mylogo.png

# glob /tmp/mylogo.png to check it is there
puts check_file_was_downloaded_successfully=[glob /tmp/mylogo.png]