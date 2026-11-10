# Local client construction and serialization only: no AWS requests.
lassign $argv root expected_version expected_abi service
if {[info tclversion] ne $expected_abi} {
    error "test interpreter [info patchlevel] does not match headers $expected_abi"
}
set services [expr {$service eq "all" ? {s3 dynamodb lambda sqs iam ssm kms ses} : [lrange $argv 3 end]}]
set module_paths {}
foreach service $services {
    lappend module_paths [file join $root aws-sdk-tcl-$service]
}
set auto_path [concat $module_paths $auto_path]
foreach service $services {
    set package aws$service
    if {[package require $package] ne $expected_version} {error "incorrect $package version"}
    set expected_library [file normalize [file join $root aws-sdk-tcl-$service libaws-sdk-tcl-$service[info sharedlibextension]]]
    if {[lsearch -exact -index 0 [info loaded] $expected_library] < 0} {
        error "$package loaded from an unexpected installation"
    }
    set create ::aws::${service}::create
    if {![catch {$create} message] || ![string match {wrong # args:*} $message]} {
        error "$package argument checking failed"
    }
    set client [$create {region us-east-1 endpoint http://127.0.0.1:1 aws_access_key_id test aws_secret_access_key test}]
    if {![llength [info commands $client]]} {error "$package did not create a client command"}
    if {$service eq "s3"} {
        set url [::aws::s3::generate_presigned_url -expire 60 $client test-bucket test-key]
        if {![string match {*X-Amz-Signature=*} $url]} {error "S3 signing failed"}
    }
    if {$service eq "dynamodb"} {
        set binary [binary format H* 00ff80]
        set item [::aws::dynamodb::typed_item_to_simple [dict create \
            name {S café} count {N 7} enabled {BOOL true} data [list B $binary] \
            nested {M {items {L {{S first} {N 2}}}}}]]
        if {[dict get $item name] ne "café" || [dict get $item count] != 7 ||
            ![dict get $item enabled] || [dict get $item data] ne $binary ||
            [dict get $item nested items] ne {first 2}} {error "DynamoDB conversion failed"}
    }
    ::aws::${service}::destroy $client
    if {[llength [info commands $client]]} {error "$package did not destroy its client"}
    puts "$package $expected_version passed on Tcl [info patchlevel]"
}
