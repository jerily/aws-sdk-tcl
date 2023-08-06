# Run the following once before running this example:
# awslocal iam create-role --role-name lambda_exec --assume-role-policy-document '{"Version": "2012-10-17","Statement": [{ "Effect": "Allow", "Principal": {"Service": "lambda.amazonaws.com"}, "Action": "sts:AssumeRole"}]}'

package require awslambda

set dir [file dirname [dict get [info frame 0] file]]
#load [file join $dir .. build src/aws-sdk-tcl-lambda libaws-sdk-tcl-lambda.so] Aws_sdk_tcl_lambda

set config_dict [dict create region us-east-1 endpoint "http://localhost:4566"]
set client [::aws::lambda::create $config_dict]
$client create_function "my-function" [file join $dir "my-function.zip"] "index.handler" "nodejs16.x" "arn:aws:iam::000000000000:role/lambda_exec"

#puts lambda_functions=[$client list_functions]
puts lambda_function_configuration=[$client get_function "my-function"]

puts invoke_function_output=[$client invoke_function "my-function" {{"message": "Hello World!"}}]
$client delete_function "my-function"
$client destroy
