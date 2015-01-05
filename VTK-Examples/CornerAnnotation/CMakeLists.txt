
cmake_minimum_required(VERSION 2.8)

PROJECT(CornerAnnotation)

find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

add_executable(CornerAnnotation MACOSX_BUNDLE CornerAnnotation)

if(VTK_LIBRARIES)
  target_link_libraries(CornerAnnotation ${VTK_LIBRARIES})
else()
  target_link_libraries(CornerAnnotation vtkHybrid vtkWidgets)
endif()
