package require awskms

set dir [file dirname [info script]]

# This is an example of encrypting/decrypting large amounts of data using KMS
# and the envelope encryption concept:
#
#     https://docs.aws.amazon.com/kms/latest/developerguide/concepts.html#enveloping
#
# First, we generate a new encryption key using the generate_data_key method.
# This method returns a list of 2 values: plaintext data and the corresponding
# ciphertext data.
#
# After that, we can then use the plaintext data to encrypt the required payload.
# In this example, we will use openssl and its AES-256-CBC encryption.
#
# Once we got the encrypted payload, we can store it among with ciphertext data
# returned by the generate_data_key method.
#
# To decrypt the payload, we first need to obtain the encryption key. This can be
# done by decrypting ciphertext data using the decrypt method. As a result, we get
# plaintext data (the same value hat was returned by method generate_data_key
# in the beginning). After that, it is possible to use the decrypted key
# (plaintext data) to decrypt the payload.

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

# generate data key with a length of 32 bytes (for AES-256-CBC encryption)
set data_key [$kms_client generate_data_key $key 32]

set plain_key [lindex $data_key 0]
set cipher_key [lindex $data_key 1]
puts plain_key=[binary encode base64 $plain_key]
puts cipher_key=[binary encode base64 $cipher_key]

# encode secret with plain_key using openssl AES-256-CBC
set chan [open "|openssl enc -aes-256-cbc -nosalt -e -iv [string repeat 0 32] -K [binary encode hex $plain_key]" rb+]
puts -nonewline $chan $secret
close $chan write
set encrypted_secret [read $chan]
close $chan
puts encrypted_secret=[binary encode base64 $encrypted_secret]

# ---------------------------------------------------------

# Now we will use encrypted_secret and cipher_key to restore the original data.

set decrypted_key [$kms_client decrypt $cipher_key]
puts decrypted_key=[binary encode base64 $decrypted_key]

# decode secret with decrypted_key using openssl AES-256-CBC

set chan [open "|openssl enc -aes-256-cbc -nosalt -d -iv [string repeat 0 32] -K [binary encode hex $decrypted_key]" rb+]
puts -nonewline $chan $encrypted_secret
close $chan write
set decrypted_secret [read $chan]
close $chan
puts decrypted_secret=$decrypted_secret

# delete the key
$kms_client schedule_key_deletion $key 7

$kms_client destroy
