package require awss3

set dir [file dirname [info script]]

set bucket_name "my-bucket"

# if you want to use it with real AWS S3, you can use the following code:
# set config_dict [dict create region "us-east-1" aws_access_key_id "your_access_key_id" aws_secret_access_key "your_secret_access_key"]
# if you want to use it with localstack, you can use the following code:
set config_dict [dict create endpoint "http://s3.localhost.localstack.cloud:4566"]

::aws::s3::create $config_dict s3_client

# checks if the bucket exists already
puts exists_bucket_before=[$s3_client exists_bucket $bucket_name]

# creates the bucket
$s3_client create_bucket $bucket_name

# checks if the bucket exists after creation
puts exists_bucket_after=[$s3_client exists_bucket $bucket_name]

# checks if the object exists already
puts exists_object_before=[$s3_client exists $bucket_name "test.txt"]

# puts a text object into a file named "test.txt"
$s3_client put_text $bucket_name "test.txt" "Hello World"

# checks if the object exists after creation
puts exists_object_after=[$s3_client exists $bucket_name "test.txt"]

# lists all objects in the bucket
set lst [$s3_client ls $bucket_name]

# lists the object in the bucket before deletion
set lst [$s3_client ls $bucket_name "test.txt"]
puts lst_before_delete=$lst

# gets the text object from the bucket
set text [$s3_client get $bucket_name "test.txt"]
puts text=$text

# downloads the file from the bucket and saves it to a file named "myfile.txt"
$s3_client get $bucket_name "test.txt" myfile.txt
puts files=[glob myfile.*]

# deletes the object from the bucket
::aws::s3::delete $s3_client $bucket_name "test.txt"

# lists the object in the bucket after deletion
set lst [$s3_client ls $bucket_name "text.txt"]
puts lst_after_delete=$lst

# uploads a file to the bucket with the name "my_logo.png"
$s3_client put $bucket_name "my_logo.png" [file join $dir .. "Google_2015_logo.png"]

# downloads the file from the bucket and saves it to a file named "mylogo.png"
$s3_client get $bucket_name "my_logo.png" mylogo.png

# deletes the object from the bucket
$s3_client delete $bucket_name "my_logo.png"

# uploads two text objects to the bucket with the names "test1.txt" and "test2.txt"
$s3_client put_text $bucket_name "test1.txt" "Hello World 1"
$s3_client put_text $bucket_name "test2.txt" "Hello World 2"

# lists all objects in the bucket before deletion
puts before_batch_delete=[$s3_client ls $bucket_name]

# batch deletes the objects from the bucket
$s3_client batch_delete $bucket_name [list "test1.txt" "test2.txt"]

# lists all objects in the bucket after deletion
puts after_batch_delete=[$s3_client ls $bucket_name]

# deletes the bucket
$s3_client delete_bucket $bucket_name

# creates 3 buckets
for {set i 0} {$i < 3} {incr i} {
    set bucket_name "my-bucket-$i"
    $s3_client create_bucket $bucket_name
}

# lists all buckets before deletion
puts buckets_before_delete=[$s3_client list_buckets]

# deletes the buckets
for {set i 0} {$i < 3} {incr i} {
    set bucket_name "my-bucket-$i"
    $s3_client delete_bucket $bucket_name
}

# lists all buckets after deletion
puts buckets_after_delete=[$s3_client list_buckets]
