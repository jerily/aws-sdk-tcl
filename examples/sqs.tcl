package require awssqs

set dir [file dirname [dict get [info frame 0] file]]

set config_dict [dict create region us-east-1 endpoint "http://localhost:4566"]
set client [::aws::sqs::create $config_dict]
set queue_url1 [$client create_queue MyQueue1]
set queue_url2 [$client create_queue MyQueue2]
puts queue_url1=$queue_url1
puts queue_url2=$queue_url2
foreach queue_url [$client list_queues] {
    puts "deleting... $queue_url"
    $client delete_queue $queue_url
}
$client destroy
