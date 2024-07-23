# TCL KMS Examples

See the [examples](examples) directory for examples of using the AWS KMS with the AWS SDK for Tcl.

# TCL KMS Commands

* **::aws::kms::create** *config_dict*
    - returns a handle to a KMS client
    - *config_dict* is a dictionary with the following keys:
      - *region* - the region name
      - *aws_access_key_id* - the access key id
      - *aws_secret_access_key* - the secret access key
      - *aws_session_token* - the session token
* **::aws::kms::list_keys**
    - returns a list of available KMS keys
* **::aws::kms::create_key**
    - creates a KMS key and returns its ARN
* **::aws::kms::describe_key** *arn*
    - returns a dict with description of a KMS key
* **::aws::kms::enable_key** *arn*
    - enables a KMS key
* **::aws::kms::disable_key** *arn*
    - disabled a KMS key
* **::aws::kms::schedule_key_deletion** *arn ?pending_window_in_days?*
    - schedule a KMS key deletion. pending_window_in_days is number of days after which the key will be deleted. It must be between 7 and 30 inclusive.
* **::aws::kms::cancel_key_deletion** *arn*
    - cancels scheduled KMS key deletion
* **::aws::kms::encrypt** *arn plain_data*
    - encrypts plaintext of up to 4096 bytes using a KMS key
* **::aws::kms::decrypt** *cipher_data*
    - decrypts ciphertext that was encrypted by a KMS key using **::aws::kms::encrypt** or **::aws::kms::generate_data_key**
* **::aws::kms::generate_data_key** *arn number_of_bytes*
    - returns a unique symmetric data key of size *number_of_bytes* for use outside of KMS. Outcome is a list of 2 values:
      - plaintext data that can be used outside of KMS
      - ciphertext data that can be decrypted by the **::aws::kms::decrypt** procedure to obtain plaintext data
* **::aws::kms::generate_random** *number_of_bytes*
    - returns a random byte string of size *number_of_bytes* that is cryptographically secure
* **::aws::kms::destroy**
    - destroys a KMS client

## Links

* [AWS CLI Command Reference for KMS](https://docs.aws.amazon.com/cli/latest/reference/kms/)
* [AWS SDK for C++ for KMS](https://sdk.amazonaws.com/cpp/api/aws-cpp-sdk-kms/html/annotated.html)
