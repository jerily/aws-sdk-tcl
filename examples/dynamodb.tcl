set dir [file dirname [dict get [info frame 0] file]]
load [file join $dir .. build src/aws-sdk-tcl-dynamodb libaws-sdk-tcl-dynamodb.so] Aws_sdk_tcl_dynamodb

set table "MyTable"

set config_dict [dict create profile localstack region us-east-1 endpoint "http://localhost:4566"]
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

$client destroy