message("metal-renderer: using cmake version: '" ${CMAKE_VERSION} "'")
project(metal-renderer VERSION 0.1.0)

# create "metal-renderer" target library
add_library(${PROJECT_NAME}) 

# specify "metal-renderer" library
target_sources(${PROJECT_NAME} PRIVATE 
source/metal-renderer.cpp
include/metal-renderer.h
) 

# directories for included files (1st/3rd party libraries)
target_include_directories(${PROJECT_NAME} PRIVATE
${PROJECT_SOURCE_DIR}/include	
${CMAKE_SOURCE_DIR}/metal-cpp	
)

# link static libraries (3rd party)
target_link_libraries(${PROJECT_NAME} 
metal-cpp
)

message("Adding ${PROJECT-NAME}")
