# aws-sdk-tcl

```bash
cd aws-sdk-cpp
mkdir build
cd build
cmake .. -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_ONLY="s3;transfer;sts"
cmake --build . --config=Release
cmake --install . --config=Release
```
