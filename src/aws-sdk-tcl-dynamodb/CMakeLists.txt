cmake_minimum_required(VERSION 3.22.1)
project(aws-sdk-tcl-dynamodb VERSION 1.0.0 LANGUAGES CXX C)
message(project: ${PROJECT_NAME})

set(TARGET ${PROJECT_NAME})
set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_C_STANDARD_REQUIRED true)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "-DTCL_THREADS -DVERSION=${PROJECT_VERSION} ${CMAKE_CXX_FLAGS}")

add_library(${PROJECT_NAME} MODULE library.cc)
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

include_directories(aws/dynamodb ${TCL_INCLUDE_PATH})
link_directories(aws/dynamodb)
target_link_libraries(aws-sdk-tcl-dynamodb PRIVATE aws-cpp-sdk-core aws-cpp-sdk-dynamodb ${TCL_LIBRARY})
get_filename_component(TCL_LIBRARY_PATH "${TCL_LIBRARY}" PATH)

install(TARGETS ${TARGET}
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${TARGET}${PROJECT_VERSION}
)

configure_file(pkgIndex.tcl.in pkgIndex.tcl @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${TARGET}${PROJECT_VERSION}
)
