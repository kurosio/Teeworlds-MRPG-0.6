# Locate MariaDB Connector/C and Connector/C++.
# Prefer a system installation; use the repository's versioned bundle as fallback.

set(MARIADB_BUNDLED_ROOT "${PROJECT_SOURCE_DIR}/libraries/mariadb")
set(MARIADB_BUNDLED_LIBDIR "${MARIADB_BUNDLED_ROOT}/${TARGET_OS}/lib${TARGET_BITS}")
set(MARIADB_BUNDLED_INCLUDEDIR "${MARIADB_BUNDLED_ROOT}/include")

set(MARIADB_CONFIG_INCLUDE_DIRS)
set(MARIADB_CONFIG_LIBRARY_PATH)
if(NOT CMAKE_CROSSCOMPILING)
  find_program(MARIADB_CONFIG NAMES mariadb_config)
  if(MARIADB_CONFIG)
    execute_process(
      COMMAND "${MARIADB_CONFIG}" --include
      OUTPUT_VARIABLE _MARIADB_INCLUDE_FLAGS
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE _MARIADB_INCLUDE_RESULT
    )
    if(_MARIADB_INCLUDE_RESULT EQUAL 0)
      string(REGEX MATCHALL "-I[^ \t]+" _MARIADB_INCLUDE_OPTIONS "${_MARIADB_INCLUDE_FLAGS}")
      foreach(_option IN LISTS _MARIADB_INCLUDE_OPTIONS)
        string(REGEX REPLACE "^-I" "" _directory "${_option}")
        list(APPEND MARIADB_CONFIG_INCLUDE_DIRS "${_directory}")
      endforeach()
    endif()

    execute_process(
      COMMAND "${MARIADB_CONFIG}" --libs_r
      OUTPUT_VARIABLE _MARIADB_LIBRARY_FLAGS
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE _MARIADB_LIBRARY_RESULT
    )
    if(_MARIADB_LIBRARY_RESULT EQUAL 0)
      string(REGEX MATCHALL "-L[^ \t]+" _MARIADB_LIBRARY_OPTIONS "${_MARIADB_LIBRARY_FLAGS}")
      foreach(_option IN LISTS _MARIADB_LIBRARY_OPTIONS)
        string(REGEX REPLACE "^-L" "" _directory "${_option}")
        list(APPEND MARIADB_CONFIG_LIBRARY_PATH "${_directory}")
      endforeach()
    endif()
  endif()
endif()

# Prefer a system Connector/C++ installation. `mariadb_config` supplies precise
# system hints when available; CMake's normal system search paths are checked
# before the bundled fallback paths listed below.
set(_MARIADB_LIBRARY_HINTS ${MARIADB_CONFIG_LIBRARY_PATH})
set(_MARIADB_LIBRARY_PATHS "${MARIADB_BUNDLED_LIBDIR}")
set(_MARIADB_INCLUDE_HINTS ${MARIADB_CONFIG_INCLUDE_DIRS})
set(_MARIADB_INCLUDE_PATHS "${MARIADB_BUNDLED_INCLUDEDIR}")

find_library(MARIADB_CORE_LIBRARY
  NAMES mariadb mariadbclient libmariadb libmariadbclient
  HINTS ${_MARIADB_LIBRARY_HINTS}
  PATHS ${_MARIADB_LIBRARY_PATHS}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)
find_library(MARIADB_LIBRARY
  NAMES mariadbcpp mariadbcpp-static libmariadbcpp libmariadbcpp-static
  HINTS ${_MARIADB_LIBRARY_HINTS}
  PATHS ${_MARIADB_LIBRARY_PATHS}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)
find_path(MARIADB_INCLUDEDIR
  NAMES mariadb/conncpp.hpp
  HINTS ${_MARIADB_INCLUDE_HINTS}
  PATHS ${_MARIADB_INCLUDE_PATHS}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MariaDB
  REQUIRED_VARS MARIADB_CORE_LIBRARY MARIADB_LIBRARY MARIADB_INCLUDEDIR
)
mark_as_advanced(MARIADB_CORE_LIBRARY MARIADB_LIBRARY MARIADB_INCLUDEDIR)

if(MARIADB_FOUND)
  is_bundled(MARIADB_BUNDLED "${MARIADB_LIBRARY}")
  set(MARIADB_LIBRARIES "${MARIADB_LIBRARY}" "${MARIADB_CORE_LIBRARY}")
  set(MARIADB_INCLUDE_DIRS "${MARIADB_INCLUDEDIR}")

  # The bundled Connector/C++ header has a case typo in its filename:
  # conncpp.hpp includes DatabaseMetaData.hpp, while the archive ships
  # DatabaseMEtaData.hpp. This works on Windows' case-insensitive filesystem
  # but fails on Linux. Add a build-tree compatibility header without touching
  # the submodule checkout.
  set(_MARIADB_METADATA_HEADER "${MARIADB_INCLUDEDIR}/mariadb/conncpp/DatabaseMetaData.hpp")
  set(_MARIADB_METADATA_HEADER_TYPO "${MARIADB_INCLUDEDIR}/mariadb/conncpp/DatabaseMEtaData.hpp")
  if(NOT EXISTS "${_MARIADB_METADATA_HEADER}" AND EXISTS "${_MARIADB_METADATA_HEADER_TYPO}")
    set(_MARIADB_COMPAT_INCLUDE_DIR "${PROJECT_BINARY_DIR}/mariadb-include-compat")
    file(MAKE_DIRECTORY "${_MARIADB_COMPAT_INCLUDE_DIR}/conncpp")
    configure_file(
      "${_MARIADB_METADATA_HEADER_TYPO}"
      "${_MARIADB_COMPAT_INCLUDE_DIR}/conncpp/DatabaseMetaData.hpp"
      COPYONLY
    )
    list(APPEND MARIADB_INCLUDE_DIRS
      "${_MARIADB_COMPAT_INCLUDE_DIR}"
      "${MARIADB_INCLUDEDIR}/mariadb/conncpp"
    )
  endif()

  # Connector/C++ needs Connector/C at runtime on Windows. Ship both bundled
  # DLLs alongside the server instead of leaving the transitive DLL missing.
  set(MARIADB_COPY_FILES)
  if(MARIADB_BUNDLED AND TARGET_OS STREQUAL "windows")
    foreach(_dll mariadbcpp.dll libmariadb.dll)
      set(_path "${MARIADB_BUNDLED_LIBDIR}/${_dll}")
      if(EXISTS "${_path}")
        list(APPEND MARIADB_COPY_FILES "${_path}")
      endif()
    endforeach()
  endif()
endif()