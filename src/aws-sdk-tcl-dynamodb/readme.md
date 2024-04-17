# TCL DynamoDB Commands
* **::aws::dynamodb::create** *config_dict*
    - returns a handle to a DynamoDB client
    - *config_dict* is a dictionary with the following keys:
      - *region* - the region name
      - *aws_access_key_id* - the access key id
      - *aws_secret_access_key* - the secret access key
      - *aws_session_token* - the session token
* **::aws::dynamodb::put_item** *handle table item_dict*
    - puts an item into a table
* **::aws::dynamodb::get_item** *handle table key_dict*
    - gets an item from a table
* **::aws::dynamodb::query_items** *handle table query_dict ?projection_expression? ?scan_forward? ?limit? ?index_name?*
    - queries items from a table
* **::aws::dynamodb::scan** *handle table ?projection_expression?*
    - scans items from a table
* **::aws::dynamodb::create_table** *handle table key_schema_dict ?provisioned_throughput_dict? ?global_secondary_indexes_list?*
    - creates a table
* **::aws::dynamodb::delete_table** *handle table*
    - deletes a table
* **::aws::dynamodb::list_tables** *handle*
    - returns a list of tables
* **::aws::dynamodb::destroy** *handle*
    - destroys a DynamoDB client
* **::aws::dynamodb::typed_item_to_simple** *typed_item*
    - converts a typed item to a simple item (TCL dict)