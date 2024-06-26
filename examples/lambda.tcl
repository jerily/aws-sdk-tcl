package require awslambda
package require awsiam

set dir [file dirname [dict get [info frame 0] file]]
#load [file join $dir .. build src/aws-sdk-tcl-lambda libaws-sdk-tcl-lambda.so] Aws_sdk_tcl_lambda

set config_dict [dict create region us-east-1 endpoint "http://localhost:4566"]
::aws::iam::create $config_dict iam_client
$iam_client create_role lambda_exec {{"Version": "2012-10-17","Statement": [{ "Effect": "Allow", "Principal": {"Service": "lambda.amazonaws.com"}, "Action": "sts:AssumeRole"}]}}

::aws::lambda::create $config_dict client
$client create_function "my-function" [file join $dir "my-function.zip"] "index.handler" "nodejs16.x" "arn:aws:iam::000000000000:role/lambda_exec"

#puts lambda_functions=[$client list_functions]
puts lambda_function_configuration=[$client get_function "my-function"]

puts invoke_function_output=[$client invoke_function "my-function" {{"message": "Hello World!"}}]
$client delete_function "my-function"

# client is destroyed via trace var, otherwise:
# $client destroy


$iam_client delete_role lambda_exec

# client is destroyed via trace var, otherwise:
# $iam_client destroy
