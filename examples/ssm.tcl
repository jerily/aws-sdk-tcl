package require awsssm

set config_dict [dict create region us-east-1 endpoint "http://localhost:4566"]
::aws::ssm::create $config_dict client
$client put_parameter "/test/parameter" "test value" "String"
puts test_parameter=[$client get_parameter "/test/parameter"]
$client delete_parameter "/test/parameter"
if { [catch {$client get_parameter "/test/parameter"} err] } {
    puts "Parameter deleted"
} else {
    puts "Parameter not deleted"
}

$client put_parameter "/test/password" "secret value" "SecureString"
puts password_parameter=[$client get_parameter "/test/password" true]
$client delete_parameter "/test/password"

# client is destroyed via trace var, otherwise:
# $client destroy
