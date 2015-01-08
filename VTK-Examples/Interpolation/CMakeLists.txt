
cmake_minimum_required(VERSION 2.8)

PROJECT(Interpolation)

find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

add_executable(Interpolation MACOSX_BUNDLE Interpolation)

if(VTK_LIBRARIES)
  target_link_libraries(Interpolation ${VTK_LIBRARIES})
else()
  target_link_libraries(Interpolation vtkHybrid vtkWidgets)
endif()
