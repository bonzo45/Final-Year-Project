
cmake_minimum_required(VERSION 2.8)

PROJECT(TriangulateTerrainMap)

find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

add_executable(TriangulateTerrainMap MACOSX_BUNDLE TriangulateTerrainMap)

if(VTK_LIBRARIES)
  target_link_libraries(TriangulateTerrainMap ${VTK_LIBRARIES})
else()
  target_link_libraries(TriangulateTerrainMap vtkHybrid vtkWidgets)
endif()
