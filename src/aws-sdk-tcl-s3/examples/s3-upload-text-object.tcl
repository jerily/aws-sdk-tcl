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

# puts a text object into a file named "test.txt"
$s3_client put_text $bucket_name "test.txt" "Hello World"

# lists all objects in the bucket
puts files_in_the_bucket=[$s3_client ls $bucket_name]
