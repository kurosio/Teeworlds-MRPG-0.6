set_extra_dirs_lib(BOX2D box2d)
find_file(BOX2D_LIBRARY box2d.lib
        HINTS ${HINTS_BOX2D_LIBDIR}
        PATHS ${PATHS_BOX2D_LIBDIR}
        ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
        )

set_extra_dirs_include(BOX2D box2d "${BOX2D_LIBRARY}")
find_path(BOX2D_INCLUDEDIR box2d/box2d.h
        HINTS ${HINTS_BOX2D_INCLUDEDIR}
        PATHS ${PATHS_BOX2D_INCLUDEDIR}
        ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
        )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Box2D DEFAULT_MSG BOX2D_INCLUDEDIR BOX2D_LIBRARY)
mark_as_advanced(BOX2D_INCLUDEDIR BOX2D_LIBRARY)

if(BOX2D_FOUND)
    is_bundled(BOX2D_BUNDLED "${BOX2D_LIBRARY}")
    set(BOX2D_LIBRARIES ${BOX2D_LIBRARY})
    set(BOX2D_INCLUDE_DIRS ${BOX2D_INCLUDEDIR})

    set(BOX2D_COPY_FILES)
    if(BOX2D_BUNDLED AND TARGET_OS STREQUAL "windows")
        set(BOX2D_COPY_FILES "${EXTRA_BOX2D_LIBDIR}/box2d.dll")
    endif()
endif()
