package require awsdynamodb

set table "MyTableToBeDeleted"

# To use it with real AWS S3, you can use the following configuration:
# set config_dict [dict create region "us-east-1" aws_access_key_id "your_access_key_id" aws_secret_access_key "your_secret_access_key"]

# To use it with localstack, you can use the following configuration:
set config_dict [dict create region us-east-1 endpoint "http://localhost:4566"]

# creates a DynamoDB client
::aws::dynamodb::create $config_dict client

# create a new table with an id attribute of type N (number) and HASH key type
$client create_table $table [dict create id [list N HASH]]

# list table before deleting the new table
puts tables_before=[$client list_tables]

# delete the table
$client delete_table $table

# list table after deleting the new table
puts tables_after=[$client list_tables]
