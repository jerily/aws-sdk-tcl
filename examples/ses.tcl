#!/usr/bin/env tclsh
# Usage: tclsh examples/ses.tcl us-east-1 sender@example.com recipient@example.com
# Credentials come from the normal AWS SDK provider chain (environment,
# shared AWS credentials files, or an IAM role). Use a verified SES sender.
package require awsses 1.0.16

if {$argc < 3 || $argc > 5} {
    puts stderr "Usage: tclsh [file tail [info script]] region from to ?subject? ?text?"
    exit 2
}
lassign $argv region from to subject text
if {$argc < 4} {set subject "Hello from Tcl"}
if {$argc < 5} {set text "This email was sent using aws-sdk-tcl-ses."}

set ses [::aws::ses::create [dict create region $region]]
try {
    set message_id [$ses send_email [dict create \
        from $from to [list $to] subject $subject text $text]]
    puts "SES accepted message: $message_id"
} finally {
    $ses destroy
}
