# aws-sdk-tcl

TCL bindings for the AWS SDK C++. Use them in TCL or NaviServer as loadable modules.

Version 1.0.13 supports Tcl 8.6 and Tcl 9.x through Tcl stubs, including automatic
Tcl 9.1 discovery. All seven modules link against the matching stub archive.
The extension builds with AWS SDK C++ 1.11.921.

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
git clone --depth 1 --branch 1.11.921 --recurse-submodules --shallow-submodules https://github.com/aws/aws-sdk-cpp
cd aws-sdk-cpp
mkdir build
cd build
cmake .. \
  -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_ONLY="s3;dynamodb;lambda;sqs;iam;transfer;sts;ssm;kms" \
  -DENABLE_TESTING=OFF \
  -DAUTORUN_UNIT_TESTS=OFF
cmake --build . --config=Release
cmake --install . --config=Release
```

## Build TCL AWS SDK

### Download the latest release
```bash
wget https://github.com/jerily/aws-sdk-tcl/archive/refs/tags/v1.0.13.tar.gz
tar -xzf v1.0.13.tar.gz
export TCL_AWS_DIR=$(pwd)/aws-sdk-tcl-1.0.13
```

### Build for TCL:

Shared modules use Tcl stubs and support Tcl 8.6 and Tcl 9.x, including 9.1.
Build separately against the headers for each Tcl major version. The finder
reads the header version and selects the matching stub archive, so a shared Tcl
runtime library is not needed at link time. Install the Tcl development package
containing `tcl.h` and the stub archive.

```bash
cd ${TCL_AWS_DIR}
mkdir build
cd build
cmake .. -DAWS_SDK_CPP_DIR=/usr/local
# or if TCL is not in the default path (/usr/local/lib):
# change "TCL_LIBRARY_DIR" and "TCL_INCLUDE_DIR" to the correct paths
# cmake .. -DAWS_SDK_CPP_DIR=/usr/local -DTCL_LIBRARY_DIR=/usr/local/lib -DTCL_INCLUDE_DIR=/usr/local/include
make
make install
```

For a custom Tcl installation, set `TCL_INCLUDE_DIR` and `TCL_LIBRARY_DIR`, or
provide `TCL_INCLUDE_PATH` and `TCL_STUB_LIBRARY` explicitly. Tcl 8.6 normally
uses `libtclstub8.6.a`; Tcl 9 normally uses `libtclstub.a`. `CMAKE_PREFIX_PATH`
is also supported. For example, to build against Tcl 8.6 on Debian/Ubuntu:

```bash
cmake -S . -B build86 -DAWS_SDK_CPP_DIR=/usr/local \
  -DTCL_INCLUDE_DIR=/usr/include/tcl8.6 \
  -DTCL_LIBRARY_DIR=/usr/lib/x86_64-linux-gnu \
  -DTCL_TCLSH=/usr/bin/tclsh8.6
cmake --build build86 --parallel 2
ctest --test-dir build86 --output-on-failure
```

The tests load all seven modules, construct and destroy clients with dummy
credentials, sign an S3 URL, and convert DynamoDB values without AWS requests.
Linux tests also check that the modules have no direct Tcl runtime dependency.
Set `TCL_TCLSH` to the matching interpreter for testing, or use
`-DBUILD_TESTING=OFF` to build without an interpreter. Package indexes reject
the wrong Tcl major version before loading the module.

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
cd ${TCL_AWS_DIR}/src/aws-sdk-tcl-kms
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

If you want to see examples with comments for a specific functionality, see Documentation section below.

## Documentation

* [TCL S3 Commands](./src/aws-sdk-tcl-s3/) - [TCL S3 Examples](./src/aws-sdk-tcl-s3/examples/)
* [TCL DynamoDB Commands](./src/aws-sdk-tcl-dynamodb/) - [TCL DynamoDB Examples](./src/aws-sdk-tcl-dynamodb/examples/)
* [TCL Lambda](./src/aws-sdk-tcl-lambda/)
* [TCL SQS](./src/aws-sdk-tcl-sqs/)
* [TCL IAM](./src/aws-sdk-tcl-iam/)
* [TCL SSM](./src/aws-sdk-tcl-ssm/)
* [TCL KMS](./src/aws-sdk-tcl-kms/) - [TCL KMS Examples](./src/aws-sdk-tcl-kms/examples/)
