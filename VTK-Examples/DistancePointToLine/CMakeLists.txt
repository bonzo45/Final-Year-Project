
cmake_minimum_required(VERSION 2.8)

PROJECT(DistancePointToLine)

find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

add_executable(DistancePointToLine MACOSX_BUNDLE DistancePointToLine)

if(VTK_LIBRARIES)
  target_link_libraries(DistancePointToLine ${VTK_LIBRARIES})
else()
  target_link_libraries(DistancePointToLine vtkHybrid vtkWidgets)
endif()
