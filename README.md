# AWS service bindings for Tcl

Build all eight services by default, or select only the bindings you need:

```sh
cmake -S . -B build -DAWS_SDK_CPP_DIR=/path/to/sdk \
  '-DAWS_SDK_TCL_SERVICES=s3;dynamodb'
cmake --build build
ctest --test-dir build --output-on-failure
```

Supported services are `s3`, `dynamodb`, `lambda`, `iam`, `sqs`, `ssm`, `kms`, and `ses`.
The selected AWS C++ libraries must already be installed. CMake imports their
targets, including their transitive dependencies for static SDK builds. Tcl
extensions remain shared libraries linked to Tcl stubs.

Registry packages such as `aws-sdk-tcl-s3` install individual bindings;
`aws-sdk-tcl` installs the complete set. Tcl package names and commands stay the
same: for example, `package require awss3` and `package require awsdynamodb`.
An individual package installs only its own library and `pkgIndex.tcl`.

SES is available as `aws-sdk-tcl-ses` (`package require awsses`). It supports
text/HTML, raw MIME and templated email, identity verification and DKIM,
template management, sending quotas and statistics. See the
[SES API reference](src/aws-sdk-tcl-ses/readme.md) and
[send-email example](examples/ses.tcl).
The `aws-sdk-cpp-ses` registry package builds upstream's `email` component;
select `ses` in this project's CMake options, or `email` in the C++ SDK's
`BUILD_ONLY` option. This binding uses the SES classic API, not SESv2.

Tests load each selected binding and all selected bindings together, and check
Tcl stub linkage. SES tests also require Python 3 and use a loopback HTTP server
with fake credentials; no email is sent. Use matching Tcl headers, stubs, and interpreter; see
`cmake/FindTclStub.cmake` for explicit paths. `-DBUILD_TESTING=OFF` disables tests.
