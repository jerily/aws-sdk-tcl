package require awsssm

set config_dict [dict create region us-east-1 endpoint "http://localhost:4566"]
set client [::aws::ssm::create $config_dict]
$client put_parameter "/test/parameter" "test value" "String"
puts test_parameter=[$client get_parameter "/test/parameter"]
#$client delete_parameter "/test/parameter"
#puts test_parameter=[$client get_parameter "/test/parameter"]
$client destroy
