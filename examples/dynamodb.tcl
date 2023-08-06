package require awsdynamodb

set dir [file dirname [dict get [info frame 0] file]]
#load [file join $dir .. build src/aws-sdk-tcl-dynamodb libaws-sdk-tcl-dynamodb.so] Aws_sdk_tcl_dynamodb

set table "MyTable"

set config_dict [dict create region us-east-1 endpoint "http://localhost:4566"]
set client [::aws::dynamodb::create $config_dict]

puts tables_before=[$client list_tables]
$client create_table $table [dict create id [list N HASH]]
puts tables_after=[$client list_tables]

set item_dict [dict create \
    id [list N 1] \
    name [list S "test"] \
    age [list N 20] \
    scopes [list L [list [list S "read"] [list S "write"] [list S "admin"]]] \
    address [list M [list \
        street [list S "1234 Main Street"] \
        city [list S "Seattle"] \
        state [list S "WA"] \
        zip [list N 98101] \
    ]] \
]
puts $item_dict
$client put_item $table $item_dict

$client put_item $table [dict create id [list N 2] name [list S "test2"]]

set key_dict [dict create id [list N "1"]]
set item [$client get_item $table $key_dict]
puts item=$item

$client delete_table $table

$client create_table NewTable [dict create id [list N HASH] ts [list N RANGE]]
$client put_item NewTable [dict create id [list N 1] ts [list N 1234567890]]
$client put_item NewTable [dict create id [list N 1] ts [list N 1234567891]]
$client put_item NewTable [dict create id [list N 1] ts [list N 1234567892]]
$client put_item NewTable [dict create id [list N 1] ts [list N 1234567893]]
$client put_item NewTable [dict create id [list N 2] ts [list N 1234567894]]
$client put_item NewTable [dict create id [list N 2] ts [list N 1234567895]]
$client put_item NewTable [dict create id [list N 2] ts [list N 1234567896]]

puts query_items_1=[$client query NewTable [dict create id [list N 1]]]
puts query_items_1_backward=[$client query NewTable [dict create id [list N 1]] false]
puts query_items_1_backward_limit_2=[$client query NewTable [dict create id [list N 1]] false 2]
puts query_items_2=[$client query NewTable [dict create id [list N 2]] true 2]
puts query_items_id_and_timestamp=[$client query NewTable [dict create id [list N 1] ts [list N 1234567890]]]
$client delete_table NewTable

# global secondary indexes
set gsi_table "MyTableWithGSI"
$client create_table $gsi_table \
    {id {S HASH} otherId S} \
    {} \
    [list [list IndexName MyTableByOtherId KeySchema [list {otherId HASH}]]]

$client put_item $gsi_table [dict create id [list S "1"] otherId [list S "a"]]
$client put_item $gsi_table [dict create id [list S "2"] otherId [list S "b"]]
$client put_item $gsi_table [dict create id [list S "3"] otherId [list S "c"]]
$client put_item $gsi_table [dict create id [list S "4"] otherId [list S "d"]]
$client put_item $gsi_table [dict create id [list S "5"] otherId [list S "e"]]

puts gsi_query_items_id_2=[$client query $gsi_table [dict create id [list S "2"]]]
puts gsi_query_items_otherid_c=[$client query $gsi_table [dict create otherId [list S "c"]] true 1 MyTableByOtherId]
$client delete_table $gsi_table

$client destroy