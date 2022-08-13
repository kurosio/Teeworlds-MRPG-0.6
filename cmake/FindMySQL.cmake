set(MYSQL_CPPCONN_LIBRARY)
set(Boost_INCLUDE_DIRS)

if(NOT CMAKE_CROSSCOMPILING)
  find_program(MYSQL_CONFIG
    NAMES mysql_config mariadb_config
  )

  if(MYSQL_CONFIG)
    exec_program(${MYSQL_CONFIG}
      ARGS --include
      OUTPUT_VARIABLE MY_TMP
    )

    string(REGEX REPLACE "-I([^ ]*)( .*)?" "\\1" MY_TMP "${MY_TMP}")

    set(MYSQL_CONFIG_INCLUDE_DIR ${MY_TMP} CACHE FILEPATH INTERNAL)

    exec_program(${MYSQL_CONFIG}
      ARGS --libs_r
      OUTPUT_VARIABLE MY_TMP
    )

    set(MYSQL_CONFIG_LIBRARIES "")

    string(REGEX MATCHALL "-l[^ ]*" MYSQL_LIB_LIST "${MY_TMP}")
    foreach(LIB ${MYSQL_LIB_LIST})
      string(REGEX REPLACE "[ ]*-l([^ ]*)" "\\1" LIB "${LIB}")
      list(APPEND MYSQL_CONFIG_LIBRARIES "${LIB}")
    endforeach()

    set(MYSQL_CONFIG_LIBRARY_PATH "")

    string(REGEX MATCHALL "-L[^ ]*" MYSQL_LIBDIR_LIST "${MY_TMP}")
    foreach(LIB ${MYSQL_LIBDIR_LIST})
      string(REGEX REPLACE "[ ]*-L([^ ]*)" "\\1" LIB "${LIB}")
      list(APPEND MYSQL_CONFIG_LIBRARY_PATH "${LIB}")
    endforeach()
  endif()
endif()

set_extra_dirs_lib(MYSQL mysql)
find_library(MYSQL_LIBRARY
  NAMES "mysqlcppconn" "mysqlcppconn-static"
  HINTS ${MYSQL_CONFIG_LIBRARY_PATH}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)
set_extra_dirs_include(MYSQL mysql "${MYSQL_LIBRARY}")
find_path(MYSQL_INCLUDEDIR
  NAMES "mysql_connection.h"
  HINTS ${MYSQL_CONFIG_INCLUDE_DIR}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MySQL DEFAULT_MSG MYSQL_LIBRARY MYSQL_INCLUDEDIR) 
set(Boost_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/libraries/boost/include")

if(NOT(MYSQL_FOUND))
  find_library(MYSQL_LIBRARY
    NAMES "mysqlcppconn" "mysqlcppconn-static"
    HINTS ${HINTS_MYSQL_LIBDIR} ${MYSQL_CONFIG_LIBRARY_PATH}
    PATHS ${PATHS_MYSQL_LIBDIR}
    ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
  )
  set_extra_dirs_include(MYSQL mysql "${MYSQL_LIBRARY}")
  find_path(MYSQL_INCLUDEDIR
    NAMES "mysql_connection.h"
    HINTS ${HINTS_MYSQL_INCLUDEDIR} ${MYSQL_CONFIG_INCLUDE_DIR}
    PATHS ${PATHS_MYSQL_INCLUDEDIR}
    ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
  )

  find_package_handle_standard_args(MySQL DEFAULT_MSG MYSQL_LIBRARY MYSQL_INCLUDEDIR)

  if(MYSQL_FOUND)
    if(TARGET_OS AND TARGET_OS STREQUAL "windows")
      set(MYSQL_COPY_FILES "${EXTRA_MYSQL_LIBDIR}/mysqlcppconn-7-vs14.dll")
    else()
      set(MYSQL_COPY_FILES)
    endif()
  endif()
endif()

set(MYSQL_LIBRARIES ${MYSQL_LIBRARY})
set(MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDEDIR} ${Boost_INCLUDE_DIRS})

mark_as_advanced(MYSQL_INCLUDEDIR MYSQL_LIBRARY)