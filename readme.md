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
```bash
mkdir build
cd build
#cmake .. -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release
cmake .. -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Debug
tclsh ../example.tcl
```