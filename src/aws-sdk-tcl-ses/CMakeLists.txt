cmake_minimum_required(VERSION 3.22.1)
project(aws-sdk-tcl-ses VERSION ${AWS_SDK_TCL_VERSION} LANGUAGES CXX C)

add_library(${PROJECT_NAME} SHARED library.cc ../common/common.cc)
set_target_properties(${PROJECT_NAME} PROPERTIES
    POSITION_INDEPENDENT_CODE ON INSTALL_RPATH_USE_LINK_PATH ON)
target_include_directories(${PROJECT_NAME} PRIVATE ${TCL_INCLUDE_PATH})
target_link_libraries(${PROJECT_NAME} PRIVATE
    aws-cpp-sdk-core aws-cpp-sdk-email ${TCL_STUB_LIBRARY})
target_compile_definitions(${PROJECT_NAME} PRIVATE
    USE_TCL_STUBS TCL_THREADS VERSION=${PROJECT_VERSION})

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib/${PROJECT_NAME}${PROJECT_VERSION})
configure_file(pkgIndex.tcl.in pkgIndex.tcl @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl
    DESTINATION lib/${PROJECT_NAME}${PROJECT_VERSION})
