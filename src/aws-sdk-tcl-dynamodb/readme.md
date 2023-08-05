awslocal dynamodb create-table \
--table-name MyTable \
--attribute-definitions \
AttributeName=id,AttributeType=N \
--key-schema \
AttributeName=id,KeyType=HASH \
--billing-mode PAY_PER_REQUEST