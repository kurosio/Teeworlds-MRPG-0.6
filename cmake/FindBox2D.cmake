set(BOX2D_BUNDLED ON)
file(GLOB_RECURSE BOX2D_SRC "src/engine/external/box2d/src/*.c" "src/engine/external/box2d/src/*.h")

set(BOX2D_INCLUDEDIR "src/engine/external/box2d/include")
add_library(box2d EXCLUDE_FROM_ALL OBJECT ${BOX2D_SRC})
target_include_directories(box2d PUBLIC ${BOX2D_INCLUDEDIR})

set(BOX2D_DEP $<TARGET_OBJECTS:box2d>)
set(BOX2D_INCLUDE_DIRS ${BOX2D_INCLUDEDIR})
set(BOX2D_LIBRARIES)
list(APPEND TARGETS_DEP box2d)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BOX2D DEFAULT_MSG BOX2D_INCLUDEDIR)