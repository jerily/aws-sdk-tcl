cmake_minimum_required(VERSION 3.22.1)
project(aws-sdk-tcl-sqs VERSION 1.0.4 LANGUAGES CXX C)
message(project: ${PROJECT_NAME})

set(TARGET ${PROJECT_NAME})
set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_C_STANDARD_REQUIRED true)
set(THREADS_PREFER_PTHREAD_FLAG ON)

list(APPEND CMAKE_MODULE_PATH "${TOPLEVEL_SOURCE_DIR}/cmake")
find_package(TCL 8.6.13 REQUIRED)  # TCL_INCLUDE_PATH TCL_LIBRARY

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "-DTCL_THREADS -DVERSION=${PROJECT_VERSION} ${CMAKE_CXX_FLAGS}")

add_library(aws-sdk-tcl-sqs SHARED library.cc
        ../common/common.cc)
set_target_properties(aws-sdk-tcl-sqs
        PROPERTIES POSITION_INDEPENDENT_CODE ON
        INSTALL_RPATH_USE_LINK_PATH ON
)
include_directories(${AWS_SDK_CPP_DIR}/include/aws/sqs ${TCL_INCLUDE_PATH})
target_link_directories(${PROJECT_NAME} PRIVATE ${AWS_SDK_CPP_DIR}/lib)
target_link_libraries(aws-sdk-tcl-sqs PRIVATE aws-cpp-sdk-core aws-cpp-sdk-sqs ${TCL_LIBRARY})
get_filename_component(TCL_LIBRARY_PATH "${TCL_LIBRARY}" PATH)

install(TARGETS ${TARGET}
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${TARGET}${PROJECT_VERSION}
)

configure_file(pkgIndex.tcl.in pkgIndex.tcl @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${TARGET}${PROJECT_VERSION}
)
