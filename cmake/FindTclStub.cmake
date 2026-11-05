# Find Tcl 8.6 or 9.x headers and the stub archive matching their ABI.
# Inputs: CMAKE_PREFIX_PATH, TCL_INCLUDE_DIR/TCL_LIBRARY_DIR, or explicit
# TCL_INCLUDE_PATH/TCL_STUB_LIBRARY. TCL_LIBRARY is not needed by extensions.
# Outputs: TclStub_FOUND, TCL_VERSION, TCL_ABI_VERSION, TCL_INCLUDE_PATH,
# TCL_STUB_LIBRARY, and TCL_TCLSH (optional unless tests are enabled).

if(TCL_INCLUDE_DIR)
    find_path(TCL_INCLUDE_PATH NAMES tcl.h PATHS "${TCL_INCLUDE_DIR}"
        PATH_SUFFIXES tcl9.1 tcl9.0 tcl8.7 tcl8.6 tcl NO_DEFAULT_PATH)
else()
    find_path(TCL_INCLUDE_PATH NAMES tcl.h
        PATH_SUFFIXES tcl9.1 tcl9.0 tcl8.7 tcl8.6 tcl)
endif()

if(EXISTS "${TCL_INCLUDE_PATH}/tcl.h")
    file(STRINGS "${TCL_INCLUDE_PATH}/tcl.h" tcl_patch_line
        REGEX "^#[ \t]*define[ \t]+TCL_PATCH_LEVEL[ \t]+\"[^\"]+\"")
    string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" TCL_VERSION "${tcl_patch_line}")
endif()

set(tcl_supported FALSE)
if(TCL_VERSION MATCHES "^([89])\\.([0-9]+)")
    set(TCL_ABI_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
    set(tcl_compact_version "${CMAKE_MATCH_1}${CMAKE_MATCH_2}")
    if(NOT TCL_VERSION VERSION_LESS 8.6)
        set(tcl_supported TRUE)
    endif()
    get_filename_component(tcl_prefix "${TCL_INCLUDE_PATH}" DIRECTORY)
    set(tcl_stub_names tclstub${TCL_ABI_VERSION} tclstub${tcl_compact_version})
    if(NOT TCL_VERSION VERSION_LESS 9)
        list(PREPEND tcl_stub_names tclstub)
    endif()
    if(TCL_LIBRARY_DIR)
        # An explicit installation must not silently fall back to a host library.
        find_library(TCL_STUB_LIBRARY NAMES ${tcl_stub_names}
            PATHS "${TCL_LIBRARY_DIR}" PATH_SUFFIXES tcl${TCL_ABI_VERSION}
            NO_DEFAULT_PATH)
    else()
        find_library(TCL_STUB_LIBRARY NAMES ${tcl_stub_names}
            HINTS "${tcl_prefix}/lib" "${tcl_prefix}/lib64"
            PATH_SUFFIXES tcl${TCL_ABI_VERSION})
    endif()
    find_program(TCL_TCLSH
        NAMES tclsh${TCL_ABI_VERSION} tclsh${tcl_compact_version} tclsh${tcl_compact_version}t
        HINTS "${tcl_prefix}/bin")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TclStub
    REQUIRED_VARS TCL_INCLUDE_PATH TCL_STUB_LIBRARY tcl_supported
    VERSION_VAR TCL_VERSION
    REASON_FAILURE_MESSAGE "Install matching Tcl 8.6 or 9.x development headers and stubs, or set TCL_INCLUDE_PATH and TCL_STUB_LIBRARY.")
mark_as_advanced(TCL_INCLUDE_PATH TCL_STUB_LIBRARY TCL_TCLSH)
