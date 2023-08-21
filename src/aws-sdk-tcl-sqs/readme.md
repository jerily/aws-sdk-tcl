# TCL SQS Commands

* **::aws::sqs::create** *config_dict*
    - returns a handle to an SQS client
* **::aws::sqs::create_queue** *handle* *queue_name*
  - creates an SQS queue
* **::aws::sqs::delete_queue** *handle* *queue_url*
  - deletes an SQS queue
* **::aws::sqs::list_queues** *handle*
  - lists all SQS queues
* **::aws::sqs::send_message** *handle* *queue_url* *message*
  - sends a message to an SQS queue
* **::aws::sqs::receive_messages** *handle* *queue_url* *?max_number_of_messages?*
  - receives messages from an SQS queue
* **::aws::sqs::destroy** *handle*
  - destroys an SQS client

