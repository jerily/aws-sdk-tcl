foreach(library IN LISTS LIBRARIES)
    execute_process(COMMAND "${READELF}" -d "${library}"
        RESULT_VARIABLE status OUTPUT_VARIABLE dynamic ERROR_VARIABLE error)
    if(NOT status EQUAL 0 OR dynamic MATCHES "NEEDED[^\n]*libtcl")
        message(FATAL_ERROR "${library}: unexpected Tcl runtime dependency or readelf failure: ${error}\n${dynamic}")
    endif()
    execute_process(COMMAND "${NM}" -D --undefined-only "${library}"
        RESULT_VARIABLE status OUTPUT_VARIABLE symbols ERROR_VARIABLE error)
    if(NOT status EQUAL 0 OR symbols MATCHES "[ \t]Tcl_[A-Za-z0-9_]+")
        message(FATAL_ERROR "${library}: unresolved direct Tcl calls or nm failure: ${error}\n${symbols}")
    endif()
endforeach()
