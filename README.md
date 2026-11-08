# AWS service bindings for Tcl

Build all seven services by default, or select only the bindings you need:

```sh
cmake -S . -B build -DAWS_SDK_CPP_DIR=/path/to/sdk \
  '-DAWS_SDK_TCL_SERVICES=s3;dynamodb'
cmake --build build
ctest --test-dir build --output-on-failure
```

Supported services are `s3`, `dynamodb`, `lambda`, `iam`, `sqs`, `ssm`, and `kms`.
The selected AWS C++ libraries must already be installed. CMake imports their
targets, including their transitive dependencies for static SDK builds. Tcl
extensions remain shared libraries linked to Tcl stubs.

Registry packages such as `aws-sdk-tcl-s3` install individual bindings;
`aws-sdk-tcl` installs the complete set. Tcl package names and commands stay the
same: for example, `package require awss3` and `package require awsdynamodb`.
An individual package installs only its own library and `pkgIndex.tcl`.

Tests load each selected binding and all selected bindings together, and check
Tcl stub linkage. Use matching Tcl headers, stubs, and interpreter; see
`cmake/FindTclStub.cmake` for explicit paths. `-DBUILD_TESTING=OFF` disables tests.
