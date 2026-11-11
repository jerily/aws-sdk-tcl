# TCL DynamoDB Examples

See the [examples](examples) directory for examples of using the AWS DynamoDB service with the AWS SDK for Tcl.

# TCL DynamoDB Commands
* **::aws::dynamodb::create** *config_dict*
    - returns a handle to a DynamoDB client
    - *config_dict* is a dictionary with the following keys:
      - *region* - the region name
      - *aws_access_key_id* - the access key id
      - *aws_secret_access_key* - the secret access key
      - *aws_session_token* - the session token
* **::aws::dynamodb::put_item** *handle table item_dict ?options?*
    - puts an item into a table
* **::aws::dynamodb::get_item** *handle table key_dict ?consistent_read?*
    - gets an item from a table
* **::aws::dynamodb::delete_item** *handle table key_dict ?options?*
    - deletes an item from a table
* **::aws::dynamodb::query_items** *handle table query_dict ?projection_expression? ?scan_forward? ?limit? ?index_name? ?consistent_read?*
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

## Conditional writes and consistent reads (1.0.18+)

The client-command forms accept the same arguments, without `handle`:

```tcl
$client put_item Sessions {id {S token}} {
    condition_expression {attribute_not_exists(#id)}
    expression_attribute_names {#id id}
}
set item [$client get_item Sessions {id {S token}} true]
```

`put_item` and `delete_item` accept an optional dictionary containing
`condition_expression`, `expression_attribute_names` (alias/name dictionary),
and `expression_attribute_values` (placeholder/typed-value dictionary).
Condition values support scalar `S`, `N` and `BOOL` types. Conditional failures
raise Tcl errors with `-errorcode {AWS DynamoDB ConditionalCheckFailedException}`;
other write failures use `{AWS DynamoDB <service exception name>}`.

`get_item` and `query_items` optionally request strong consistency. Existing
calls retain their defaults. Query optional arguments can be empty to use the
default; an explicit limit must be positive and caps the total items returned
across pages. Omit the limit to read every page. Strong consistency is supported
on base tables and local secondary indexes, not global secondary indexes.

Run the local HTTP request tests (no AWS credentials or service needed):

```sh
python3 src/aws-sdk-tcl-dynamodb/tests/requests_server.py --library /path/to/libaws-sdk-tcl-dynamodb.so
```
