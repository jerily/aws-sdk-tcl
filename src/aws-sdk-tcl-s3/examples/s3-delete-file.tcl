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

# lists all objects in the bucket before deletion
puts files_in_the_bucket_before_deletion=[$s3_client ls $bucket_name]

# deletes the file "my_logo.png" from the bucket
$s3_client delete $bucket_name "my_logo.png"

# lists all objects in the bucket after deletion
puts files_in_the_bucket_after_deletion=[$s3_client ls $bucket_name]
