
cmake_minimum_required(VERSION 2.8)

project(WidthHeight)

find_package(ITK REQUIRED)
include(${ITK_USE_FILE})
if (ITKVtkGlue_LOADED)
  find_package(VTK REQUIRED)
  include(${VTK_USE_FILE})
endif()

add_executable(WidthHeight MACOSX_BUNDLE WidthHeight.cxx)

if( "${ITK_VERSION_MAJOR}" LESS 4 )
  target_link_libraries(WidthHeight ITKReview ${ITK_LIBRARIES})
else( "${ITK_VERSION_MAJOR}" LESS 4 )
  target_link_libraries(WidthHeight ${ITK_LIBRARIES})
endif( "${ITK_VERSION_MAJOR}" LESS 4 )

