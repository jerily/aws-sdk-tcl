# aws-sdk-tcl

## Clone the repository
```bash
git clone --depth 1 --recurse-submodules --shallow-submodules https://github.com/jerily/aws-sdk-tcl.git
cd aws-sdk-tcl
TCL_AWS_DIR=$(pwd)
```

## Build Dependencies

### AWS SDK C++ Dependencies

To install the packages on Debian/Ubuntu-based systems:
```bash
sudo apt-get install libcurl4-openssl-dev libssl-dev uuid-dev zlib1g-dev libpulse-dev
```

To install the packages on Amazon Linux/Redhat/Fedora/CentOS-based systems
```bash
sudo yum install libcurl-devel openssl-devel libuuid-devel pulseaudio-libs-devel
```

### AWS SDK C++ Build
```bash
cd ${TCL_AWS_DIR}/aws-sdk-cpp
mkdir build
cd build
cmake .. -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_ONLY="s3;dynamodb;lambda;sqs;iam;transfer;sts"
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
make
make install
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
# Depending on your setup you may need to set LD_LIBRARY_PATH
# export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
tclsh ${TCL_AWS_DIR}/examples/s3.tcl
tclsh ${TCL_AWS_DIR}/examples/dynamodb.tcl
tclsh ${TCL_AWS_DIR}/examples/lambda.tcl
tclsh ${TCL_AWS_DIR}/examples/sqs.tcl
tclsh ${TCL_AWS_DIR}/examples/iam.tcl
```

## Documentation

* [TCL S3](./src/aws-sdk-tcl-s3/)
* [TCL DynamoDB](./src/aws-sdk-tcl-dynamodb/)
* [TCL Lambda](./src/aws-sdk-tcl-lambda/)
* [TCL SQS](./src/aws-sdk-tcl-sqs/)
* [TCL IAM](./src/aws-sdk-tcl-iam/)
