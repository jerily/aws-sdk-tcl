set dir [file dirname [dict get [info frame 0] file]]
load [file join $dir .. build src/aws-sdk-tcl-dynamodb libaws-sdk-tcl-dynamodb.so] Aws_sdk_tcl_dynamodb

set client [::aws::dynamodb::create [dict create]]
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

set table "my-table"
$client put_item $table $item_dict
$client destroy