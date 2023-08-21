# TCL S3 Commands

* **::aws::s3::create** *config_dict*
    - returns a handle to an SQS client
* **::aws::s3::create_queue** *handle* *queue_name*
  - creates an SQS queue
* **::aws::s3::delete_queue** *handle* *queue_url*
  - deletes an SQS queue
* **::aws::s3::list_queues** *handle*
  - lists all SQS queues
* **::aws::s3::destroy** *handle*
  - destroys an SQS client

