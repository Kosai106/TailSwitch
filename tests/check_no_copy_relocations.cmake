# Qt's protected external data symbols on SteamOS reject ELF copy relocations.
execute_process(
    COMMAND "${READELF}" --relocs --wide "${EXECUTABLE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE relocations
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect executable relocations: ${error}")
endif()
if(relocations MATCHES "R_[A-Za-z0-9_]*_COPY[ \t]")
    message(FATAL_ERROR "Executable has ELF copy relocations; build consumers with -fPIC for SteamOS Qt compatibility")
endif()
