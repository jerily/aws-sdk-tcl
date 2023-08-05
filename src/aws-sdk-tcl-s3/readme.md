# TCL S3 Commands

* **::aws::s3::create** *config_dict*
    - returns a handle to an S3 client
* **::aws::s3::ls** *handle bucket ?key?*
    - returns a list of objects in a bucket
* **::aws::s3::put_text** *handle bucket key text*
    - puts a string into an object
* **::aws::s3::put** *handle_name bucket key filename*
    - puts a file into an object
* **::aws::s3::get** *handle_name bucket key ?filename?*
    - gets an object and returns a string
* **::aws::s3::delete** *handle_name bucket key*
    - deletes an object
* **::aws::s3::batch_delete** *handle_name bucket keys*
    - deletes a list of objects
* **::aws::s3::exists** *handle_name bucket key*
    - returns true if an object exists
* **::aws::s3::create_bucket** *handle_name bucket*
    - creates a bucket
* **::aws::s3::delete_bucket** *handle_name bucket*
    - deletes an empty bucket
* **::aws::s3::exists_bucket** *handle_name bucket*
    - returns true if a bucket exists
* **::aws::s3::list_buckets** *handle_name*
    - returns a list of buckets
* **::aws::s3::destroy** *handle*
    - destroys an S3 client

