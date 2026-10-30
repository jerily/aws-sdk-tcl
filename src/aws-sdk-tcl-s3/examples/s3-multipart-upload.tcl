package require awss3 1.0.11

if {$argc != 3} {
    puts stderr "Usage: tclsh [info script] bucket key filename"
    exit 1
}
lassign $argv bucket key filename

# Uses the SDK's default credentials provider. The bucket must already exist.
set config [dict create region us-east-1]
# For LocalStack, also set endpoint http://s3.localhost.localstack.cloud:4566.
set client [::aws::s3::create $config]
try {
    # The helper reads one part at a time and attempts cleanup on failure.
    puts "File ETag: [$client put_multipart $bucket $key $filename]"

    # Upload a second object manually, preserving binary bytes and ETag quotes.
    set manual_key "${key}.manual"
    set channel [open $filename rb]
    try {
        set upload_id [$client create_multipart_upload $bucket $manual_key application/octet-stream]
        try {
            set parts {}
            set number 1
            while {1} {
                set data [read $channel 8388608]
                if {[string length $data] == 0 && $number > 1} {
                    break
                }
                set etag [$client upload_part $bucket $manual_key $upload_id $number $data]
                lappend parts [list $number $etag]
                incr number
                if {[eof $channel]} {
                    break
                }
            }
            puts "Manual ETag: [$client complete_multipart_upload $bucket $manual_key $upload_id $parts]"
        } on error {message options} {
            if {[catch {$client abort_multipart_upload $bucket $manual_key $upload_id} cleanup_error]} {
                puts stderr "Could not abort upload $upload_id: $cleanup_error"
            }
            return -options $options $message
        }
    } finally {
        close $channel
    }
} finally {
    $client destroy
}
