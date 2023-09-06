# TCL SSM Commands
* **::aws::ssm::create** *config_dict*
  - returns a handle to an ssm client
* **::aws::ssm::destroy** *handle*
  - destroys an ssm client
* **::aws::ssm::put_parameter** *handle* *name* *value* *type*
  - puts a parameter into the parameter store
* **::aws::ssm::get_parameter** *handle* *name*
  - gets a parameter from the parameter store
* **::aws::ssm::delete_parameter** *handle* *name*
  - deletes a parameter from the parameter store