package require awsiam

set dir [file dirname [dict get [info frame 0] file]]
#load [file join $dir .. build src/aws-sdk-tcl-iam libaws-sdk-tcl-iam.so] Aws_sdk_tcl_iam

set config_dict [dict create region us-east-1 endpoint "http://localhost:4566"]
::aws::iam::create $config_dict client
set iam_create_role_output [$client create_role lambda_exec {{"Version": "2012-10-17","Statement": [{ "Effect": "Allow", "Principal": {"Service": "lambda.amazonaws.com"}, "Action": "sts:AssumeRole"}]}}]
puts iam_create_role_output=$iam_create_role_output
puts iam_list_policies=[$client list_policies]
$client delete_role lambda_exec

# client is destroyed via trace var, otherwise:
# $client destroy
