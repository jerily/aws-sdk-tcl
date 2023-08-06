set dir [file dirname [dict get [info frame 0] file]]
load [file join $dir .. build src/aws-sdk-tcl-lambda libaws-sdk-tcl-lambda.so] Aws_sdk_tcl_lambda

set client [::aws::lambda::create [dict create]]
puts lambda_functions=[$client list_functions]
$client destroy
