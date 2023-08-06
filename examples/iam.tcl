# Run the following once before running this example:
# awslocal iam create-role --role-name lambda_exec --assume-role-policy-document '{"Version": "2012-10-17","Statement": [{ "Effect": "Allow", "Principal": {"Service": "lambda.amazonaws.com"}, "Action": "sts:AssumeRole"}]}'

#package require awslambda

set dir [file dirname [dict get [info frame 0] file]]
load [file join $dir .. build src/aws-sdk-tcl-iam libaws-sdk-tcl-iam.so] Aws_sdk_tcl_iam

set config_dict [dict create region us-east-1 endpoint "http://localhost:4566"]
set client [::aws::iam::create $config_dict]
$client destroy
