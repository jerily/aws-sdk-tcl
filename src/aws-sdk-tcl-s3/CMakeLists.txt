cmake_minimum_required(VERSION 3.22.1)
project(aws-sdk-tcl-s3 LANGUAGES CXX C)
message(project: ${PROJECT_NAME})

set(TARGET ${PROJECT_NAME})
set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_C_STANDARD_REQUIRED true)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

#set(CMAKE_VERBOSE_MAKEFILE ON)
#set(CMAKE_CXX_FLAGS "-DDEBUG ${CMAKE_CXX_FLAGS}")

add_library(aws-sdk-tcl-s3 SHARED MODULE library.cc)
set_target_properties(aws-sdk-tcl-s3 PROPERTIES POSITION_INDEPENDENT_CODE ON)

if (NAVISERVER)
#    include_directories(bert.cpp/ggml/include bert.cpp/ ${NAVISERVER}/include)
#    link_directories(bert.cpp/build bert.cpp/build/ggml/src)
#    target_link_directories(aws-sdk-tcl-s3 PRIVATE ${NAVISERVER}/lib)
#    target_link_libraries(aws-sdk-tcl-s3 PRIVATE aws-cpp-sdk-core ${TCL_LIBRARY} nsd)
#    target_compile_options(aws-sdk-tcl-s3 PRIVATE -DUSE_NAVISERVER)
#    install(TARGETS ${TARGET}
#            LIBRARY DESTINATION ${NAVISERVER}/bin
#    )
else ()
    include_directories(aws/s3 ${TCL_INCLUDE_PATH})
    link_directories(aws/s3)
    target_link_libraries(aws-sdk-tcl-s3 PRIVATE aws-cpp-sdk-core aws-cpp-sdk-s3 ${TCL_LIBRARY})
    get_filename_component(TCL_LIBRARY_PATH "${TCL_LIBRARY}" PATH)
    if (TCL_LIBRARY_PATH)
        install(TARGETS ${TARGET}
                LIBRARY DESTINATION ${TCL_LIBRARY_PATH}/${TARGET}
        )
        install(FILES pkgIndex.tcl
                DESTINATION ${TCL_LIBRARY_PATH}/${TARGET}
        )
    endif()
endif()

