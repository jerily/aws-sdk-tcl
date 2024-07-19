package require awss3

set dir [file dirname [info script]]

set bucket_name "examplebucket.s3.amazonaws.com"

# if you want to use it with real AWS S3, you can use the following code:
# set config_dict [dict create region "us-east-1" aws_access_key_id "your_access_key_id" aws_secret_access_key "your_secret_access_key"]
# if you want to use it with localstack, you can use the following code:
set config_dict [dict create \
    region "us-east-1" \
    aws_access_key_id "AKIAIOSFODNN7EXAMPLE" \
    aws_secret_access_key "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY" \
]

::aws::s3::create $config_dict s3_client

# Generate URL for default HTTP method (GET) and default expiration time
set url [::aws::s3::generate_presigned_url $s3_client $bucket_name "file.txt"]
puts url.GET=$url

# Generate URL for HTTP method "POST" and an expiration date of 15 minutes (900 seconds)
set url [::aws::s3::generate_presigned_url -method POST -expire 900 $s3_client $bucket_name "file.txt"]
puts url.POST=$url