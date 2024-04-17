# TCL SQS Commands

* **::aws::sqs::create** *config_dict*
  - returns a handle to an SQS client
  - *config_dict* is a dictionary with the following keys:
    - *region* - the region name
    - *aws_access_key_id* - the access key id
    - *aws_secret_access_key* - the secret access key
    - *aws_session_token* - the session token
* **::aws::sqs::destroy** *handle*
  - destroys an SQS client
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
* **::aws::sqs::delete_message** *handle* *queue_url* *message_receipt_handle*
  - deletes a message from an SQS queue
* **::aws::sqs::delete_message_batch** *handle* *queue_url* *message_receipt_handles*
  - deletes a batch of messages from an SQS queue
* **::aws::sqs::change_message_visibility** *handle* *queue_url* *message_receipt_handle* *visibility_timeout_seconds*
  - changes the visibility timeout of a message
* **::aws::sqs::set_queue_attributes** *handle* *queue_url* *attributes_dict*
  - sets attributes on an SQS queue
* **::aws::sqs::get_queue_attributes** *handle* *queue_url*
  - gets all attributes of an SQS queue




