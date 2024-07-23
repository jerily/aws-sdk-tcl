package require awskms

set dir [file dirname [info script]]

# To use it with real AWS KMS, you can use the following configuration:
# set config_dict [dict create region "us-east-1" aws_access_key_id "your_access_key_id" aws_secret_access_key "your_secret_access_key"]

# To use it with localstack, you can use the following configuration:
set config_dict [dict create endpoint "http://localhost:4566"]

# creates an KMS client
::aws::kms::create $config_dict kms_client

# generate random 64 bytes
puts random_data=[binary encode base64 [$kms_client generate_random 64]]

$kms_client destroy
