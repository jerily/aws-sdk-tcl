package require awssqs

set dir [file dirname [dict get [info frame 0] file]]

set config_dict [dict create region us-east-1 endpoint "http://localhost:4566"]
set client [::aws::sqs::create $config_dict]
set queue_url [$client create_queue MyQueue]
puts queue_url=$queue_url
$client delete_queue $queue_url
$client destroy
