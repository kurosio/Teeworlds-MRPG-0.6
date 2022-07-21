set_extra_dirs_lib(MYSQL mysql)
####################################### MySQL
find_library(OWN_MYSQL_LIBRARY
  NAMES "mysqlclient" "mysqlclient_r" "mariadbclient"
  HINTS ${HINTS_MYSQL_LIBDIR} ${MYSQL_CONFIG_LIBRARY_PATH}
  PATHS ${PATHS_MYSQL_LIBDIR}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)
#######################################
set_extra_dirs_include(MYSQL mysql "${OWN_MYSQL_LIBRARY}")
find_path(OWN_MYSQL_INCLUDEDIR
  NAMES "mysql.h"
  HINTS ${HINTS_MYSQL_INCLUDEDIR} ${MYSQL_CONFIG_INCLUDE_DIR}
  PATHS ${PATHS_MYSQL_INCLUDEDIR}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)

####################################### MySQL - CPPCON
set_extra_dirs_lib(MYSQL_CPPCONN mysql)
find_library(OWN_MYSQL_CPPCONN_LIBRARY
  NAMES "mysqlcppconn" "mysqlcppconn-static"
  HINTS ${HINTS_MYSQL_CPPCONN_LIBDIR} ${MYSQL_CONFIG_LIBRARY_PATH}
  PATHS ${PATHS_MYSQL_CPPCONN_LIBDIR}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)
#######################################
set_extra_dirs_include(MYSQL_CPPCONN mysql "${OWN_MYSQL_CPPCONN_LIBRARY}")
find_path(OWN_MYSQL_CPPCONN_INCLUDEDIR
  NAMES "mysql_connection.h"
  HINTS ${HINTS_MYSQL_CPPCONN_INCLUDEDIR} ${MYSQL_CONFIG_INCLUDE_DIR}
  PATHS ${PATHS_MYSQL_CPPCONN_INCLUDEDIR}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)

#######################################
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MySQL DEFAULT_MSG OWN_MYSQL_LIBRARY OWN_MYSQL_INCLUDEDIR)

mark_as_advanced(OWN_MYSQL_LIBRARY OWN_MYSQL_INCLUDEDIR)

#######################################
if(NOT(MYSQL_FOUND))
  # [some data may not match dependencies / TODO: later fix it]
  set(CMAKE_MODULE_PATH ${ORIGINAL_CMAKE_MODULE_PATH})
  find_package(MYSQL)
  set(CMAKE_MODULE_PATH ${OWN_CMAKE_MODULE_PATH})
else()
  set(MYSQL_LIBRARIES ${OWN_MYSQL_LIBRARY} ${OWN_MYSQL_CPPCONN_LIBRARY})
  set(MYSQL_INCLUDE_DIRS ${OWN_MYSQL_INCLUDEDIR} ${OWN_MYSQL_CPPCONN_INCLUDEDIR})
  
  if(TARGET_OS AND TARGET_OS STREQUAL "windows")
    set(MYSQL_COPY_FILES "${EXTRA_MYSQL_LIBDIR}/mysqlcppconn-7-vs14.dll")
  else()
    set(MYSQL_COPY_FILES)
  endif()
endif()