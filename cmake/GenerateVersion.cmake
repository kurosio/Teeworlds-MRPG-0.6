file(READ "${CMAKE_SOURCE_DIR}/version_base.txt" BASE_FILE_CONTENTS)

string(REGEX MATCH "MAJOR=([0-9]+)" _ ${BASE_FILE_CONTENTS})
set(MAJOR_VERSION ${CMAKE_MATCH_1})

string(REGEX MATCH "MINOR=([0-9]+)" _ ${BASE_FILE_CONTENTS})
set(MINOR_VERSION ${CMAKE_MATCH_1})

string(REGEX MATCH "COMMIT=([a-f0-9]+)" _ ${BASE_FILE_CONTENTS})
set(BASE_COMMIT ${CMAKE_MATCH_1})

execute_process(
        COMMAND git rev-list --count ${BASE_COMMIT}..HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE PATCH_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

file(WRITE ${CMAKE_BINARY_DIR}/src/generated/generated_version.h.in
        "#pragma once\n"
        "#define MRPG_VERSION_MAJOR ${MAJOR_VERSION}\n"
        "#define MRPG_VERSION_MINOR ${MINOR_VERSION}\n"
        "#define MRPG_VERSION_PATCH ${PATCH_VERSION}\n"
        "#define MRPG_VERSION_STRING \"${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}\"\n"
)
configure_file(
        ${CMAKE_BINARY_DIR}/src/generated/generated_version.h.in
        ${CMAKE_BINARY_DIR}/src/generated/version.h
        @ONLY
)
