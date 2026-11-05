cmake_minimum_required(VERSION 3.22.1)
project(aws-sdk-tcl VERSION 1.0.13 LANGUAGES CXX C)
message(project: ${PROJECT_NAME})

set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_C_STANDARD_REQUIRED true)
set(THREADS_PREFER_PTHREAD_FLAG ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(TclStub 8.6 REQUIRED)
if(TCL_VERSION VERSION_LESS 9)
    set(TCL_PACKAGE_REQUIREMENT "8.6-9")
else()
    set(TCL_PACKAGE_REQUIREMENT "9.0-10")
endif()

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(TOPLEVEL_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(src/aws-sdk-tcl-s3)
add_subdirectory(src/aws-sdk-tcl-dynamodb)
add_subdirectory(src/aws-sdk-tcl-lambda)
add_subdirectory(src/aws-sdk-tcl-iam)
add_subdirectory(src/aws-sdk-tcl-sqs)
add_subdirectory(src/aws-sdk-tcl-ssm)
add_subdirectory(src/aws-sdk-tcl-kms)

include(CTest)
if(BUILD_TESTING)
    if(NOT TCL_TCLSH)
        message(FATAL_ERROR "Set TCL_TCLSH to a matching Tcl interpreter, or disable BUILD_TESTING")
    endif()
    foreach(service s3 dynamodb lambda sqs iam ssm kms all)
        add_test(NAME load_${service} COMMAND "${TCL_TCLSH}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/load.tcl"
            "${CMAKE_CURRENT_BINARY_DIR}/src" "${PROJECT_VERSION}"
            "${TCL_ABI_VERSION}" "${service}")
        set_tests_properties(load_${service} PROPERTIES
            ENVIRONMENT "AWS_EC2_METADATA_DISABLED=true" TIMEOUT 30)
    endforeach()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_READELF AND CMAKE_NM)
        set(module_libraries)
        foreach(service s3 dynamodb lambda sqs iam ssm kms)
            list(APPEND module_libraries "$<TARGET_FILE:aws-sdk-tcl-${service}>")
        endforeach()
        add_test(NAME tcl_stubs COMMAND "${CMAKE_COMMAND}"
            "-DLIBRARIES=${module_libraries}" "-DREADELF=${CMAKE_READELF}" "-DNM=${CMAKE_NM}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-stubs.cmake")
    endif()
endif()
