# TCL S3 Examples

See the [examples](examples) directory for examples of using the AWS S3 service with the AWS SDK for Tcl.

# TCL S3 Commands

* **::aws::s3::create** *config_dict*
    - returns a handle to an S3 client
    - *config_dict* is a dictionary with the following keys:
      - *region* - the region name
      - *aws_access_key_id* - the access key id
      - *aws_secret_access_key* - the secret access key
      - *aws_session_token* - the session token
* **::aws::s3::ls** *handle bucket ?key?*
    - returns a list of objects in a bucket
* **::aws::s3::put_text** *handle bucket key text*
    - puts a string into an object
* **::aws::s3::put** *handle bucket key filename*
    - puts a file into an object
* **::aws::s3::get** *handle bucket key ?filename?*
    - gets an object and returns a string
* **::aws::s3::delete** *handle bucket key*
    - deletes an object
* **::aws::s3::batch_delete** *handle bucket keys*
    - deletes a list of objects
* **::aws::s3::exists** *handle bucket key*
    - returns true if an object exists
* **::aws::s3::create_bucket** *handle bucket*
    - creates a bucket
* **::aws::s3::delete_bucket** *handle bucket*
    - deletes an empty bucket
* **::aws::s3::exists_bucket** *handle bucket*
    - returns true if a bucket exists
* **::aws::s3::list_buckets** *handle*
    - returns a list of buckets
* **::aws::s3::destroy** *handle*
    - destroys an S3 client
* **::aws::s3::generate_presigned_url** *?-method method? ?-expire seconds? handle_name bucket key*
    - returns an authenticated URL (AWS Signature Version 4)
      - *method* - the HTTP method (GET/POST/PUT etc.)
      - *seconds* - the expiration date of the generated URL
