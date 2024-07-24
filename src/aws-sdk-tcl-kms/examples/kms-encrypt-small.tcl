package require awskms

set dir [file dirname [info script]]

# KMS encrypt/decrypt API intended to operate with small amounts of data.
# It can work with up to 4096 bytes of arbitrary data such as an RSA key,
# a database password, or other sensitive customer information.
#
# An example of handling large amounts of data can be found in kms-encrypt-big.tcl

# To use it with real AWS KMS, you can use the following configuration:
# set config_dict [dict create region "us-east-1" aws_access_key_id "your_access_key_id" aws_secret_access_key "your_secret_access_key"]

# To use it with localstack, you can use the following configuration:
set config_dict [dict create endpoint "http://localhost:4566"]

# creates an KMS client
::aws::kms::create $config_dict kms_client

# create a key
set key [$kms_client create_key]
puts created_key=$key

set secret "!TOP SECRET DATA!"

puts secret=$secret

# encrypt data
set encrypted_data [$kms_client encrypt $key $secret]
puts encrypted_data=[binary encode base64 $encrypted_data]

# decrypt data
set plain_data [$kms_client decrypt $encrypted_data]
puts decrypted_data=$plain_data

$kms_client schedule_key_deletion $key 7

$kms_client destroy
