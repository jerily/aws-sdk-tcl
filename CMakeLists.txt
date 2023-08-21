cmake_minimum_required(VERSION 3.22.1)
project(aws-sdk-tcl LANGUAGES CXX C)
message(project: ${PROJECT_NAME})

set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_C_STANDARD_REQUIRED true)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

#set(CMAKE_VERBOSE_MAKEFILE ON)

include(${CMAKE_CURRENT_LIST_DIR}/FindTCL.cmake)

add_subdirectory(src/aws-sdk-tcl-s3)
add_subdirectory(src/aws-sdk-tcl-dynamodb)
add_subdirectory(src/aws-sdk-tcl-lambda)
add_subdirectory(src/aws-sdk-tcl-iam)
add_subdirectory(src/aws-sdk-tcl-sqs)
