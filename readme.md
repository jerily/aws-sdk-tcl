# aws-sdk-tcl

## Clone the repository
```bash
git clone --recurse-submodules https://github.com/jerily/aws-sdk-tcl.git
cd aws-sdk-tcl
TCL_AWS_DIR=$(pwd)
```

## Build Dependencies
```bash
cd ${TCL_AWS_DIR}/aws-sdk-cpp
mkdir build
cd build
cmake .. -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_ONLY="s3;dynamodb;lambda;sqs;transfer;sts"
cmake --build . --config=Release
cmake --install . --config=Release
```

## Build
For TCL:
```bash
cd ${TCL_AWS_DIR}
mkdir build
cd build
cmake .. -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release
```

For NaviServer:
```bash
cd ${TCL_AWS_DIR}/src/aws-sdk-tcl-s3
make
make install
cd ${TCL_AWS_DIR}/src/aws-sdk-tcl-dynamodb
make
make install
```

## Try out the examples
Install [localstack](https://docs.localstack.cloud/getting-started/installation/) and run the following commands:

```bash
LS_LOG=trace localstack start
tclsh ${TCL_AWS_DIR}/examples/s3.tcl
tclsh ${TCL_AWS_DIR}/examples/dynamodb.tcl
```

## Documentation

* [TCL S3](./src/aws-sdk-tcl-s3/)
* [TCL DynamoDB](./src/aws-sdk-tcl-dynamodb/)
