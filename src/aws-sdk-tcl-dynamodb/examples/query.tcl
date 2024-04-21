package require awsdynamodb

set table "NewTable"

# To use it with real AWS S3, you can use the following configuration:
# set config_dict [dict create region "us-east-1" aws_access_key_id "your_access_key_id" aws_secret_access_key "your_secret_access_key"]

# To use it with localstack, you can use the following configuration:
set config_dict [dict create region us-east-1 endpoint "http://localhost:4566"]

# creates a DynamoDB client
::aws::dynamodb::create $config_dict client

# creates a table with a HASH key "id" and a RANGE key "ts"
$client create_table $table [dict create id [list N HASH] ts [list N RANGE]]

# put multiple items with different ts values into the table
$client put_item $table [dict create id [list N 1] ts [list N 1234567890]]
$client put_item $table [dict create id [list N 1] ts [list N 1234567891]]
$client put_item $table [dict create id [list N 1] ts [list N 1234567892]]
$client put_item $table [dict create id [list N 1] ts [list N 1234567893]]
$client put_item $table [dict create id [list N 2] ts [list N 1234567894]]
$client put_item $table [dict create id [list N 2] ts [list N 1234567895]]
$client put_item $table [dict create id [list N 2] ts [list N 1234567896]]

# query items with id=1
puts query_items_1=[$client query $table [dict create id [list N 1]]]

# query items with id=1 with projection expression (only return "id" attribute)
puts query_items_1,with_proj_expr=[$client query $table [dict create id [list N 1]] "id"]

# query items with id=1 in a backward direction
puts query_items_1_backward=[$client query $table [dict create id [list N 1]] "" false]

# query items with id=1 in a backward direction and limit the number of items to 2
puts query_items_1_backward_limit_2=[$client query $table [dict create id [list N 1]] "" false 2]

# query items with id=2 in a forward direction and limit the number of items to 2
puts query_items_2=[$client query $table [dict create id [list N 2]] "" true 2]

# query items with id=2 and ts=1234567890
puts query_items_id_and_timestamp=[$client query $table [dict create id [list N 1] ts [list N 1234567890]]]

# scan the table with projection expression (only return "id" attribute)
puts scan_table_with_proj_expr,typed=[$client scan $table id]

# scan the table and return all attributes
puts scan_table,typed=[$client scan $table]

# scan the table and return all attributes in a simple format
puts scan_table,simple=[lmap x [$client scan $table] {::aws::dynamodb::typed_item_to_simple $x}]
