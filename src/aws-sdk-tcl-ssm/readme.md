# TCL SSM Commands
* **::aws::ssm::create** *config_dict*
  - returns a handle to an ssm client
  - *config_dict* is a dictionary with the following keys:
    - *region* - the region name
    - *aws_access_key_id* - the access key id
    - *aws_secret_access_key* - the secret access key
    - *aws_session_token* - the session token
* **::aws::ssm::destroy** *handle*
  - destroys an ssm client
* **::aws::ssm::put_parameter** *handle* *name* *value* *type*
  - puts a parameter into the parameter store
* **::aws::ssm::get_parameter** *handle* *name*
  - gets a parameter from the parameter store
* **::aws::ssm::delete_parameter** *handle* *name*
  - deletes a parameter from the parameter store