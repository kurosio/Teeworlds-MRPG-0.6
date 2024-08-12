set(LIBMAXMINDDB_DIR ${CMAKE_SOURCE_DIR}/other/libmaxminddb_external)

# Check type size
include(CheckTypeSize)
check_type_size("unsigned __int128" UINT128)
check_type_size("unsigned int __attribute__((mode(TI)))" UINT128_USING_MODE)
if(HAVE_UINT128)
  set(MMDB_UINT128_USING_MODE 0)
  set(MMDB_UINT128_IS_BYTE_ARRAY 0)
elseif(HAVE_UINT128_USING_MODE)
  set(MMDB_UINT128_USING_MODE 1)
  set(MMDB_UINT128_IS_BYTE_ARRAY 0)
else()
  set(MMDB_UINT128_USING_MODE 0)
  set(MMDB_UINT128_IS_BYTE_ARRAY 1)
endif()

# Test big endian
include (TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)

# Generate and prepare include dir
file(COPY ${LIBMAXMINDDB_DIR}/geninc/maxminddb.h DESTINATION ${CMAKE_BINARY_DIR}/libmaxminddb/include)
configure_file(${LIBMAXMINDDB_DIR}/geninc/maxminddb_config.h.cmake.in
          ${CMAKE_BINARY_DIR}/libmaxminddb/include/maxminddb_config.h)
set(LIBMAXMINDDB_INCLUDE_DIR ${CMAKE_BINARY_DIR}/libmaxminddb/include)

# add library
file(GLOB LIBMAXMINDDB_SRC_FILES
    "${LIBMAXMINDDB_DIR}/src/*.c"
    "${LIBMAXMINDDB_DIR}/src/*.h"
)
add_library(libmaxminddb EXCLUDE_FROM_ALL OBJECT ${LIBMAXMINDDB_SRC_FILES})
set_target_properties(libmaxminddb PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
if(NOT MSVC)
  target_compile_options(libmaxminddb PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wno-declaration-after-statement>)
endif()
target_compile_definitions(libmaxminddb PRIVATE PACKAGE_VERSION="1.10.0")
if(NOT IS_BIG_ENDIAN)
  target_compile_definitions(libmaxminddb PRIVATE MMDB_LITTLE_ENDIAN=1)
endif()
target_include_directories(libmaxminddb PRIVATE ${LIBMAXMINDDB_INCLUDE_DIR})
if(WIN32)
  target_link_libraries(libmaxminddb ws2_32)
endif()

# add library to targets
list(APPEND TARGETS_DEP libmaxminddb)