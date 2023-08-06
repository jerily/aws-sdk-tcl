# TCL Lambda Commands
* **::aws::lambda::create** *config_dict*
    - returns a handle to a Lambda client
* **::aws::lambda::list_functions** *handle*
    - returns a list of Lambda function configurations
* **::aws::lambda::get_function** *handle* *function_name*
    - returns a Lambda function configuration
* **::aws::lambda::create_function** *handle* *function_name* *function_code_path* *handler* *runtime* *execution_role_arn*
    - creates a Lambda function
* **::aws::lambda::invoke_function** *handle* *function_name* *payload* *?invocation_type?*
    - invokes a Lambda function
* **::aws::lambda::delete_function** *handle* *function_name*
    - deletes a Lambda function
* **::aws::lambda::destroy** *handle*
    - destroys a Lambda client handle
