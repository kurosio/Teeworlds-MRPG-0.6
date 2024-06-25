if(NOT CMAKE_CROSSCOMPILING)
    find_package(PkgConfig QUIET)
    pkg_check_modules(PC_DPP libdpp)
endif()

set_extra_dirs_lib(DPP DPP)
find_library(DPP_LIBRARY
        NAMES dpp libdpp
        HINTS ${HINTS_DPP_LIBDIR}
        PATHS ${PATHS_DPP_LIBDIR}
        ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)

set_extra_dirs_include(DPP DPP "${DPP_LIBRARY}")
find_path(DPP_INCLUDEDIR
        NAMES dpp/version.h
        HINTS ${HINTS_DPP_INCLUDEDIR}
        PATHS ${PATHS_DPP_INCLUDEDIR}
        ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DPP DEFAULT_MSG DPP_LIBRARY DPP_INCLUDEDIR)

mark_as_advanced(DPP_LIBRARY DPP_INCLUDEDIR)

if(DPP_FOUND)
    set(DPP_LIBRARIES ${DPP_LIBRARY})
    set(DPP_INCLUDE_DIRS ${DPP_INCLUDEDIR})

    is_bundled(DPP_BUNDLED "${DPP_LIBRARY}")
    set(DPP_COPY_FILES)
    if(DPP_BUNDLED)
        if(TARGET_OS STREQUAL "windows")
            file(GLOB_RECURSE DPP_COPY_FILES "${EXTRA_DPP_LIBDIR}/*.dll")
        elseif(TARGET_OS STREQUAL "linux")
            set(DPP_COPY_FILES "${EXTRA_DPP_LIBDIR}/libdpp.so")
        endif()
    endif()
endif()