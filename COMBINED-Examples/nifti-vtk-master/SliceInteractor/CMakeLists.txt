cmake_minimum_required(VERSION 2.8)

find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

add_library(SliceInteractor VtkSliceInteractorStyle.cxx StatusMessage.cxx)
target_link_libraries(SliceInteractor ${VTK_LIBRARIES})