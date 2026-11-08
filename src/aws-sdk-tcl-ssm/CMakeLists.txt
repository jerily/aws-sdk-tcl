cmake_minimum_required(VERSION 3.22.1)
project(aws-sdk-tcl-ssm VERSION ${AWS_SDK_TCL_VERSION} LANGUAGES CXX C)
message(project: ${PROJECT_NAME})

set(TARGET ${PROJECT_NAME})
set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_C_STANDARD_REQUIRED true)
set(THREADS_PREFER_PTHREAD_FLAG ON)

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "-DTCL_THREADS -DPROJECT_VERSION=${PROJECT_VERSION} ${CMAKE_CXX_FLAGS}")

add_library(${PROJECT_NAME} SHARED library.cc
        ../common/common.cc)
set_target_properties(${PROJECT_NAME}
        PROPERTIES POSITION_INDEPENDENT_CODE ON
        INSTALL_RPATH_USE_LINK_PATH ON
)
target_include_directories(${PROJECT_NAME} PRIVATE ${TCL_INCLUDE_PATH})
target_link_libraries(aws-sdk-tcl-ssm PRIVATE aws-cpp-sdk-core aws-cpp-sdk-ssm ${TCL_STUB_LIBRARY})
target_compile_definitions(${PROJECT_NAME} PRIVATE USE_TCL_STUBS)

install(TARGETS ${TARGET}
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${TARGET}${PROJECT_VERSION}
)

configure_file(pkgIndex.tcl.in pkgIndex.tcl @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${TARGET}${PROJECT_VERSION}
)
