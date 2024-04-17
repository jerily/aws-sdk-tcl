package require awssqs

set dir [file dirname [dict get [info frame 0] file]]

set config_dict [dict create region us-east-1 endpoint "http://localhost:4566"]
::aws::sqs::create $config_dict client
set queue_url1 [$client create_queue MyQueue1]
set queue_url2 [$client create_queue MyQueue2]
puts queue_url1=$queue_url1
puts queue_url2=$queue_url2

puts get_queue_attributes,queue_url1=[$client get_queue_attributes $queue_url1]

$client send_message $queue_url1 "Hello World"
$client send_message $queue_url1 "This is a test"
$client set_queue_attributes $queue_url1 [dict create VisibilityTimeout 10 ReceiveMessageWaitTimeSeconds 20]

set received_messages [$client receive_messages $queue_url1 10]
puts received_messages=$received_messages

$client change_message_visibility $queue_url1 [dict get [lindex $received_messages 0] ReceiptHandle] 40
$client change_message_visibility $queue_url1 [dict get [lindex $received_messages 1] ReceiptHandle] 0

set newly_received_messages [$client receive_messages $queue_url1 10]
puts newly_received_messages=$newly_received_messages

foreach message $newly_received_messages {
    set receipt_handle [dict get $message ReceiptHandle]
    puts "deleting message... [dict get $message Body]"
    $client delete_message $queue_url1 $receipt_handle
}

for {set i 0} {$i < 3} {incr i} {
    $client send_message $queue_url1 "Hello World $i"
}

set received_messages_to_be_deleted [$client receive_messages $queue_url1 10]
puts received_messages_to_be_deleted=[lmap x $received_messages_to_be_deleted {dict get $x Body}]

$client delete_message_batch $queue_url1 [lmap x $received_messages_to_be_deleted {dict get $x ReceiptHandle}]


set check_is_empty [$client receive_messages $queue_url1 10]
puts check_is_empty=$check_is_empty


foreach queue_url [$client list_queues] {
    puts "deleting queue... $queue_url"
    $client delete_queue $queue_url
}

# client is destroyed via trace var, otherwise:
# $client destroy
