package require awss3

set dir [file dirname [info script]]

set bucket_name "my-bucket"

# To use it with real AWS S3, you can use the following configuration:
# set config_dict [dict create region "us-east-1" aws_access_key_id "your_access_key_id" aws_secret_access_key "your_secret_access_key"]

# To use it with localstack, you can use the following configuration:
set config_dict [dict create endpoint "http://s3.localhost.localstack.cloud:4566"]

# creates an S3 client
::aws::s3::create $config_dict s3_client

# creates 3 buckets
for {set i 0} {$i < 3} {incr i} {
    set bucket_name "my-bucket-$i"
    $s3_client create_bucket $bucket_name
}

# lists all buckets before deletion
puts buckets_before_delete=[$s3_client list_buckets]

# deletes the buckets
for {set i 0} {$i < 3} {incr i} {
    set bucket_name "my-bucket-$i"
    $s3_client delete_bucket $bucket_name
}

# lists all buckets after deletion
puts buckets_after_delete=[$s3_client list_buckets]
