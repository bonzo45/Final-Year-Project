cmake_minimum_required(VERSION 2.8)
 
project(nifti-vtk)
 
find_package(ITK REQUIRED)
include(${ITK_USE_FILE})

find_package(ItkVtkGlue REQUIRED)
include(${ItkVtkGlue_USE_FILE})

find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})

file(GLOB UI_FILES *.ui)
file(GLOB QT_WRAP *.h)
file(GLOB CXX_FILES *.cxx)

QT4_WRAP_UI(UISrcs ${UI_FILES})
QT4_WRAP_CPP(MOCSrcs ${QT_WRAP})

include_directories ("${PROJECT_SOURCE_DIR}/SliceInteractor")
add_subdirectory (SliceInteractor) 

add_executable(nifti-vtk ${CXX_FILES} ${UISrcs} ${MOCSrcs})
target_link_libraries(nifti-vtk ${Glue} ${VTK_LIBRARIES} ${ITK_LIBRARIES} ${QT_LIBRARIES} SliceInteractor)

set(CMAKE_CXX_FLAGS_RELEASE "-Wall -O2")
set(CMAKE_CXX_FLAGS_DEBUG  "-Wall -O0 -g")