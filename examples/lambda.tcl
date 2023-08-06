set dir [file dirname [dict get [info frame 0] file]]
load [file join $dir .. build src/aws-sdk-tcl-lambda libaws-sdk-tcl-lambda.so] Aws_sdk_tcl_lambda

set config_dict [dict create region us-east-1 endpoint "http://localhost:4566"]
set client [::aws::lambda::create $config_dict]
#puts lambda_functions=[$client list_functions]
puts lambda_function_configuration=[$client get_function "my-function"]
$client destroy
