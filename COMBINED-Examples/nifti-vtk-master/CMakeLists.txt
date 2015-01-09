cmake_minimum_required(VERSION 2.8)
 
project(nifti-vtk)
 
find_package(ITK REQUIRED)
include(${ITK_USE_FILE})
if (ITKVtkGlue_LOADED)
  find_package(VTK REQUIRED)
  include(${VTK_USE_FILE})
else()
  find_package(ItkVtkGlue REQUIRED)
  include(${ItkVtkGlue_USE_FILE})
  set(Glue ItkVtkGlue)
endif()
 
include_directories ("${PROJECT_SOURCE_DIR}/SliceInteractor")
add_subdirectory (SliceInteractor) 

add_executable(nifti-vtk main.cxx)
target_link_libraries(nifti-vtk ${Glue} ${VTK_LIBRARIES} ${ITK_LIBRARIES} SliceInteractor)

set(CMAKE_CXX_FLAGS_RELEASE "-Wall -O2")
set(CMAKE_CXX_FLAGS_DEBUG  "-Wall -O0 -g")