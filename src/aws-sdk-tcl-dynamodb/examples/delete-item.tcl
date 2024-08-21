package require awsdynamodb

set table "MyTableWithItems"

# To use it with real AWS S3, you can use the following configuration:
# set config_dict [dict create region "us-east-1" aws_access_key_id "your_access_key_id" aws_secret_access_key "your_secret_access_key"]

# To use it with localstack, you can use the following configuration:
set config_dict [dict create region us-east-1 endpoint "http://localhost:4566"]

# creates a DynamoDB client
::aws::dynamodb::create $config_dict client

# create a new table with an id attribute of type N (number) and HASH key type
$client create_table $table [dict create id [list N HASH]]

# an item with a nested structure, it includes the following attributes:
# - id: number
# - name: string
# - age: number
# - scopes: list of strings
# - address: map with the following attributes:
#   - street: string
#   - city: string
#   - state: string
#   - zip: number
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

# put the item into the table
$client put_item $table $item_dict

# put a second item into the table
$client put_item $table [dict create id [list N 2] name [list S "test2"]]

# specify the key of the item to get
set key_dict [dict create id [list N "1"]]

# delete the item from the table
puts deleted=[$client delete_item $table $key_dict]

# get the item from the table
set item [$client get_item $table $key_dict]
if { $item eq {} } {
    puts "Item not found"
} else {
    puts "Item: $item"
}
