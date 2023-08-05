# aws-sdk-tcl

## Build Dependencies
```bash
cd aws-sdk-cpp
mkdir build
cd build
cmake .. -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_ONLY="s3;dynamodb;lambda;sqs;transfer;sts"
cmake --build . --config=Release
cmake --install . --config=Release
```

## Build
For TCL:
```bash
mkdir build
cd build
cmake .. -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release
tclsh ../example.tcl
```

## Try out the examples
```bash
LS_LOG=trace localstack start
tclsh examples/s3.tcl
tclsh examples/dynamodb.tcl
```