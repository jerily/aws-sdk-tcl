cmake_minimum_required(VERSION 3.22.1)
project(aws-sdk-tcl VERSION 1.0.16 LANGUAGES CXX C)
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

set(AWS_SDK_TCL_VERSION ${PROJECT_VERSION})
set(AWS_SDK_TCL_SUPPORTED_SERVICES s3 dynamodb lambda iam sqs ssm kms ses)
set(AWS_SDK_TCL_SERVICES "${AWS_SDK_TCL_SUPPORTED_SERVICES}" CACHE STRING
    "Semicolon-separated AWS service bindings to build")
if(NOT AWS_SDK_TCL_SERVICES)
    message(FATAL_ERROR "AWS_SDK_TCL_SERVICES must select at least one service")
endif()
list(REMOVE_DUPLICATES AWS_SDK_TCL_SERVICES)
foreach(service IN LISTS AWS_SDK_TCL_SERVICES)
    if(NOT service IN_LIST AWS_SDK_TCL_SUPPORTED_SERVICES)
        message(FATAL_ERROR "Unsupported AWS SDK Tcl service: ${service}")
    endif()
endforeach()
if(AWS_SDK_CPP_DIR)
    list(PREPEND CMAKE_PREFIX_PATH "${AWS_SDK_CPP_DIR}")
endif()
# The unmodified SDK core exports ZLIB::ZLIB for static request compression.
find_package(ZLIB REQUIRED)
set(AWS_SDK_CPP_COMPONENTS ${AWS_SDK_TCL_SERVICES})
# The SES classic API is named "email" in the upstream C++ SDK.
list(TRANSFORM AWS_SDK_CPP_COMPONENTS REPLACE "^ses$" "email")
find_package(AWSSDK REQUIRED COMPONENTS ${AWS_SDK_CPP_COMPONENTS})

set(TOPLEVEL_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
foreach(service IN LISTS AWS_SDK_TCL_SERVICES)
    add_subdirectory(src/aws-sdk-tcl-${service})
endforeach()

include(CTest)
if(BUILD_TESTING)
    if(NOT TCL_TCLSH)
        message(FATAL_ERROR "Set TCL_TCLSH to a matching Tcl interpreter, or disable BUILD_TESTING")
    endif()
    foreach(service IN LISTS AWS_SDK_TCL_SERVICES)
        add_test(NAME load_${service} COMMAND "${TCL_TCLSH}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/load.tcl"
            "${CMAKE_CURRENT_BINARY_DIR}/src" "${PROJECT_VERSION}"
            "${TCL_ABI_VERSION}" "${service}")
        set_tests_properties(load_${service} PROPERTIES
            ENVIRONMENT "AWS_EC2_METADATA_DISABLED=true" TIMEOUT 30)
    endforeach()
    add_test(NAME load_selected COMMAND "${TCL_TCLSH}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/load.tcl"
        "${CMAKE_CURRENT_BINARY_DIR}/src" "${PROJECT_VERSION}"
        "${TCL_ABI_VERSION}" ${AWS_SDK_TCL_SERVICES})
    set_tests_properties(load_selected PROPERTIES
        ENVIRONMENT "AWS_EC2_METADATA_DISABLED=true" TIMEOUT 30)
    if("ses" IN_LIST AWS_SDK_TCL_SERVICES)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        add_test(NAME ses_requests COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/aws-sdk-tcl-ses/tests/ses_server.py"
            --library "$<TARGET_FILE:aws-sdk-tcl-ses>" --tclsh "${TCL_TCLSH}")
        set_tests_properties(ses_requests PROPERTIES TIMEOUT 120)
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_READELF AND CMAKE_NM)
        set(module_libraries)
        foreach(service IN LISTS AWS_SDK_TCL_SERVICES)
            list(APPEND module_libraries "$<TARGET_FILE:aws-sdk-tcl-${service}>")
        endforeach()
        add_test(NAME tcl_stubs COMMAND "${CMAKE_COMMAND}"
            "-DLIBRARIES=${module_libraries}" "-DREADELF=${CMAKE_READELF}" "-DNM=${CMAKE_NM}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-stubs.cmake")
    endif()
endif()
