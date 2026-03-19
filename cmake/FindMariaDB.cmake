if(NOT CMAKE_CROSSCOMPILING)
  find_program(MARIADB_CONFIG
    NAMES mariadb_config
  )

  if(MARIADB_CONFIG)
    execute_process(COMMAND ${MARIADB_CONFIG} --include OUTPUT_VARIABLE MY_TMP)
    string(REGEX REPLACE "-I([^ ]*)( .*)?" "\\1" MY_TMP "${MY_TMP}")
    set(MARIADB_CONFIG_INCLUDE_DIR ${MY_TMP} CACHE FILEPATH INTERNAL)

    execute_process(COMMAND ${MARIADB_CONFIG} --libs_r OUTPUT_VARIABLE MY_TMP)
    set(MARIADB_CONFIG_LIBRARIES "")
    string(REGEX MATCHALL "-l[^ ]*" MARIADB_LIB_LIST "${MY_TMP}")
    foreach(LIB ${MARIADB_LIB_LIST})
      string(REGEX REPLACE "[ ]*-l([^ ]*)" "\\1" LIB "${LIB}")
      list(APPEND MARIADB_CONFIG_LIBRARIES "${LIB}")
    endforeach()

    set(MARIADB_CONFIG_LIBRARY_PATH "")
    string(REGEX MATCHALL "-L[^ ]*" MARIADB_LIBDIR_LIST "${MY_TMP}")
    foreach(LIB ${MARIADB_LIBDIR_LIST})
      string(REGEX REPLACE "[ ]*-L([^ ]*)" "\\1" LIB "${LIB}")
      list(APPEND MARIADB_CONFIG_LIBRARY_PATH "${LIB}")
    endforeach()
  endif()
endif()

set_extra_dirs_lib(MARIADB mariadb)
find_library(MARIADB_CORE_LIBRARY
  NAMES "mariadb" "mariadbclient"
  PATHS ${PATHS_MARIADB_LIBDIR}
  HINTS ${HINTS_MARIADB_LIBDIR} ${MARIADB_CONFIG_LIBRARY_PATH}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)
find_library(MARIADB_LIBRARY
  NAMES "mariadbcpp" "mariadbcpp-static"
  PATHS ${PATHS_MARIADB_LIBDIR}
  HINTS ${HINTS_MARIADB_LIBDIR}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)
set_extra_dirs_include(MARIADB mariadb "${MARIADB_LIBRARY}")
find_path(MARIADB_INCLUDEDIR
        NAMES "mariadb/conncpp.hpp"
        PATHS ${PATHS_MARIADB_INCLUDEDIR}
        HINTS ${HINTS_MARIADB_INCLUDEDIR}
        ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MariaDB DEFAULT_MSG MARIADB_CORE_LIBRARY MARIADB_LIBRARY MARIADB_INCLUDEDIR)
mark_as_advanced(MARIADB_CORE_LIBRARY MARIADB_LIBRARY MARIADB_INCLUDEDIR)

if(MARIADB_FOUND)
  is_bundled(MARIADB_BUNDLED "${MARIADB_LIBRARY}")
  set(MARIADB_LIBRARIES ${MARIADB_CORE_LIBRARY} ${MARIADB_LIBRARY})
  set(MARIADB_INCLUDE_DIRS ${MARIADB_INCLUDEDIR})

  if(MARIADB_BUNDLED AND TARGET_OS STREQUAL "windows")
    set(MARIADB_COPY_FILES
            "${EXTRA_MARIADB_LIBDIR}/mariadbcpp.dll"
    )
  else()
    set(MARIADB_COPY_FILES)
  endif()
endif()