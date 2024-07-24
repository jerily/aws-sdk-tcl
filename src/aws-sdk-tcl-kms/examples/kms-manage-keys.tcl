package require awskms

set dir [file dirname [info script]]

# To use it with real AWS KMS, you can use the following configuration:
# set config_dict [dict create region "us-east-1" aws_access_key_id "your_access_key_id" aws_secret_access_key "your_secret_access_key"]

# To use it with localstack, you can use the following configuration:
set config_dict [dict create endpoint "http://localhost:4566"]

# creates an KMS client
::aws::kms::create $config_dict kms_client

# lists all keys
puts keys_before_actions=[$kms_client list_keys]

# create a key
set key [$kms_client create_key]
puts created_key=$key

# describe the key
puts describe_key=[$kms_client describe_key $key]

# disable the key
puts disable_key=[$kms_client disable_key $key]

puts describe_key_after_disabling=[$kms_client describe_key $key]

# enable the key
puts enable_key=[$kms_client enable_key $key]

puts describe_key_after_enabling=[$kms_client describe_key $key]

# schedule deletion of the key
puts schedule_key_deletion=[$kms_client schedule_key_deletion $key]

puts describe_key_after_deletion=[$kms_client describe_key $key]

# cancel deletion of the key
puts cancel_key_deletion=[$kms_client cancel_key_deletion $key]

puts describe_key_after_cancel_deletion=[$kms_client describe_key $key]

# schedule deletion of the key after 7 days
puts schedule_key_deletion=[$kms_client schedule_key_deletion $key 7]

puts describe_key_after_deletion2=[$kms_client describe_key $key]

$kms_client destroy
