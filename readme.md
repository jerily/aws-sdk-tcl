# aws-sdk-tcl

TCL bindings for the AWS SDK C++. Use them in TCL or NaviServer as loadable modules.

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

To install the packages on MacOS:
```bash
brew install openssl@3
```

### AWS SDK C++ Build
```bash
git clone --depth 1 --branch 1.11.157 --recurse-submodules --shallow-submodules https://github.com/aws/aws-sdk-cpp
cd aws-sdk-cpp
mkdir build
cd build
cmake .. \
  -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_ONLY="s3;dynamodb;lambda;sqs;iam;transfer;sts;ssm" \
  -DENABLE_TESTING=OFF \
  -DAUTORUN_UNIT_TESTS=OFF
cmake --build . --config=Release
cmake --install . --config=Release
```

## Build TCL AWS SDK

### Download the latest release
```bash
wget https://github.com/jerily/aws-sdk-tcl/archive/refs/tags/v1.0.3.tar.gz
tar -xzf v1.0.3.tar.gz
export TCL_AWS_DIR=$(pwd)/aws-sdk-tcl-1.0.3
```

### Build for TCL:
```bash
cd ${TCL_AWS_DIR}
mkdir build
cd build
# change "TCL_LIBRARY_DIR" and "TCL_INCLUDE_DIR" to the correct paths
cmake .. \
  -DTCL_LIBRARY_DIR=/usr/local/lib \
  -DTCL_INCLUDE_DIR=/usr/local/include \
  -DAWS_SDK_CPP_DIR=/usr/local
make
make install
```

### Build for NaviServer:
```bash
cd ${TCL_AWS_DIR}/src/aws-sdk-tcl-s3
make
make install
cd ${TCL_AWS_DIR}/src/aws-sdk-tcl-dynamodb
make
make install
cd ${TCL_AWS_DIR}/src/aws-sdk-tcl-lambda
make
make install
cd ${TCL_AWS_DIR}/src/aws-sdk-tcl-sqs
make
make install
cd ${TCL_AWS_DIR}/src/aws-sdk-tcl-iam
make
make install
cd ${TCL_AWS_DIR}/src/aws-sdk-tcl-ssm
make
make install
```

## Try out the examples
Install [localstack](https://docs.localstack.cloud/getting-started/installation/) and run the following commands:

```bash
LS_LOG=trace localstack start
# Depending on your setup you may need to set LD_LIBRARY_PATH
# export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
tclsh8.6 ${TCL_AWS_DIR}/examples/s3.tcl
tclsh8.6 ${TCL_AWS_DIR}/examples/dynamodb.tcl
tclsh8.6 ${TCL_AWS_DIR}/examples/lambda.tcl
tclsh8.6 ${TCL_AWS_DIR}/examples/sqs.tcl
tclsh8.6 ${TCL_AWS_DIR}/examples/iam.tcl
tclsh8.6 ${TCL_AWS_DIR}/examples/ssm.tcl
```

## Documentation

* [TCL S3](./src/aws-sdk-tcl-s3/)
* [TCL DynamoDB](./src/aws-sdk-tcl-dynamodb/)
* [TCL Lambda](./src/aws-sdk-tcl-lambda/)
* [TCL SQS](./src/aws-sdk-tcl-sqs/)
* [TCL IAM](./src/aws-sdk-tcl-iam/)
* [TCL SSM](./src/aws-sdk-tcl-ssm/)
