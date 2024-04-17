# TCL IAM Commands
* **::aws::iam::create** *config_dict*
    - returns a handle to a iam client
    - *config_dict* is a dictionary with the following keys:
      - *region* - the region name
      - *aws_access_key_id* - the access key id
      - *aws_secret_access_key* - the secret access key
      - *aws_session_token* - the session token
* **::aws::iam::create_role** *handle* *role_name* *assume_role_policy_document*
    - creates a iam role
* **::aws::iam::delete_role** *handle* *role_name*
    - deletes a iam role
* **::aws::iam::list_policies** *handle*
    - lists all policies
