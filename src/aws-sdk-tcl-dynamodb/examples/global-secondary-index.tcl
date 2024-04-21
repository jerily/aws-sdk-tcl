package require awsdynamodb

# To use it with real AWS S3, you can use the following configuration:
# set config_dict [dict create region "us-east-1" aws_access_key_id "your_access_key_id" aws_secret_access_key "your_secret_access_key"]

# To use it with localstack, you can use the following configuration:
set config_dict [dict create region us-east-1 endpoint "http://localhost:4566"]

# creates a DynamoDB client
::aws::dynamodb::create $config_dict client

# global secondary index table
set gsi_table "MyTableWithGSI"

# create a table with a global secondary index
# the table has a primary key "id" and a global secondary index "MyTableByOtherId" with a key "otherId"
$client create_table $gsi_table \
    {id {S HASH} otherId S} \
    {} \
    [list [list IndexName MyTableByOtherId KeySchema [list {otherId HASH}]]]

# puts some items in the table
$client put_item $gsi_table [dict create id [list S "1"] otherId [list S "a"]]
$client put_item $gsi_table [dict create id [list S "2"] otherId [list S "b"]]
$client put_item $gsi_table [dict create id [list S "3"] otherId [list S "c"]]
$client put_item $gsi_table [dict create id [list S "4"] otherId [list S "d"]]
$client put_item $gsi_table [dict create id [list S "5"] otherId [list S "e"]]

# query the table using the primary key
puts gsi_query_items_id_2=[$client query $gsi_table [dict create id [list S "2"]]]

# query the table using the global secondary index
puts gsi_query_items_otherid_c=[$client query $gsi_table [dict create otherId [list S "c"]] "" true 1 MyTableByOtherId]
