package require awsssm

set client [::aws::ssm::create [dict create]]
$client put_parameter "/test/parameter" "test value" "String"
puts test_parameter=[$client get_parameter "/test/parameter"]
#$client delete_parameter "/test/parameter"
#puts test_parameter=[$client get_parameter "/test/parameter"]
$client destroy
