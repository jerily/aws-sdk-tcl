# TCL S3 Commands

* **::aws::s3::create** *config_dict*
    - returns a handle to an SQS client
* **::aws::s3::create_queue** *handle* *queue_name*
  - creates an SQS queue
* **::aws::s3::delete_queue** *handle* *queue_url*
  - deletes an SQS queue
* **::aws::s3::list_queues** *handle*
  - lists all SQS queues
* **::aws::s3::send_message** *handle* *queue_url* *message*
  - sends a message to an SQS queue
* **::aws::s3::receive_messages** *handle* *queue_url* *?max_number_of_messages?*
  - receives messages from an SQS queue
* **::aws::s3::destroy** *handle*
  - destroys an SQS client

