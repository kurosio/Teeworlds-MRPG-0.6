if(NOT PREFER_BUNDLED_LIBS)
  set(CMAKE_MODULE_PATH ${ORIGINAL_CMAKE_MODULE_PATH})
  find_package(Box2D)
  set(CMAKE_MODULE_PATH ${OWN_CMAKE_MODULE_PATH})
  if(Box2D_FOUND)
    set(Box2D_BUNDLED OFF)
    set(Box2D_DEP)
  endif()
endif()

if(NOT Box2D_FOUND)
  set(Box2D_BUNDLED ON)
  file(GLOB_RECURSE Box2D_SRC "src/engine/external/box2d/src/*.c" "src/engine/external/box2d/src/*.h")

  set(Box2D_INCLUDEDIR "src/engine/external/box2d/include")
  add_library(box2d EXCLUDE_FROM_ALL OBJECT ${Box2D_SRC})
  target_include_directories(box2d PUBLIC ${Box2D_INCLUDEDIR})

  set(Box2D_DEP $<TARGET_OBJECTS:box2d>)
  set(Box2D_INCLUDE_DIRS ${Box2D_INCLUDEDIR})
  set(Box2D_LIBRARIES)
  list(APPEND TARGETS_DEP box2d)
	  
  set_target_properties(box2d PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  
  if(MSVC)
      target_compile_options(box2d PRIVATE /wd4566)
  else()
      target_compile_options(box2d PRIVATE -Wno-declaration-after-statement)
  endif()

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Box2D DEFAULT_MSG Box2D_INCLUDEDIR)
endif()