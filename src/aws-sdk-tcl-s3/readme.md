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
* **::aws::s3::put_multipart** *handle bucket key filename ?part_size?*
    - uploads a file sequentially in parts and returns the completed object's ETag
    - *part_size* is in bytes, defaults to 8 MiB (8388608), and must be between 5 MiB and 5 GiB
    - buffers one part at a time; increase *part_size* if the file would require more than 10000 parts
    - supports empty files and attempts to abort the upload on a read, part upload, or completion failure
    - if abort also fails, the error includes the upload ID for manual cleanup
* **::aws::s3::create_multipart_upload** *handle bucket key ?content_type?*
    - starts an upload and returns its upload ID; optional *content_type* sets the object's MIME type
* **::aws::s3::upload_part** *handle bucket key upload_id part_number data*
    - uploads a Tcl byte array and returns the part's ETag, including its quotes
    - *part_number* must be between 1 and 10000; reusing a number replaces that part
    - *data* is binary, not a filename; use a binary channel to read a file, or `encoding convertto utf-8` for text
* **::aws::s3::complete_multipart_upload** *handle bucket key upload_id parts*
    - completes an upload and returns the completed object's ETag
    - *parts* is a list of `{part_number etag}` pairs, e.g. `[list [list 1 $etag1] [list 2 $etag2]]`
    - sorts parts by number and rejects empty lists, duplicate numbers, and empty ETags
* **::aws::s3::abort_multipart_upload** *handle bucket key upload_id*
    - aborts an unfinished upload and returns an empty string
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

All commands are also available as client methods: `$s3_client put_multipart $bucket $key $filename`.

## Multipart uploads

### Why use multipart uploads?

Multipart uploads split a large object into independently uploaded, numbered
parts. S3 assembles the object when you complete the upload. The main motivation
is reliable large-file transfers: if a part fails, it can be retried without
resending the parts that already succeeded. AWS recommends considering multipart
uploads when an object reaches about 100 MB. See the
[AWS multipart upload guide](https://docs.aws.amazon.com/AmazonS3/latest/userguide/mpuoverview.html).

The lifecycle commands let applications save the upload ID and part ETags to
resume an unfinished upload, or coordinate concurrent part uploads for higher
throughput. Applications must manage that saved state and concurrency themselves.
`put_multipart` provides a simpler sequential workflow: it buffers one part at a
time, uses the AWS SDK's request retry behavior, and attempts to abort the upload
if it cannot finish. It does not automatically resume uploads across calls.

For small files, `put` remains simpler and uses fewer requests. Multipart uploads
require initiation and completion requests in addition to the part requests.
Unfinished parts also consume billable storage until the upload is completed or
aborted, so cleanup is part of both example workflows.

### Choosing a workflow

Use `put_multipart` for a complete file upload, or use the four lifecycle commands
to control individual parts. Keep the upload ID and each returned ETag until the
upload completes. For manual uploads, handle failures by retrying parts or calling
`abort_multipart_upload`; destroying the client does not abort outstanding uploads.

S3 requires every part except the last to be at least 5 MiB, with at most 10000
parts and 5 GiB per part. Minimum size is enforced by S3 at completion, since
`upload_part` cannot know which part will be last. See the
[AWS multipart limits](https://docs.aws.amazon.com/AmazonS3/latest/userguide/qfacts.html).
See [s3-multipart-upload.tcl](examples/s3-multipart-upload.tcl) for both workflows.

## Tests

After building, run the local HTTP integration tests with a Tcl interpreter that
matches the headers used to compile the extension:

```bash
python3 src/aws-sdk-tcl-s3/tests/multipart_server.py \
  --tclsh tclsh8.6 --library build/src/aws-sdk-tcl-s3/libaws-sdk-tcl-s3.so
```

The tests use a temporary local S3 test server and dummy credentials; no AWS
account or LocalStack installation is needed.
