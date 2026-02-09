project(metal-app VERSION 0.1.0)

# Logging
message("metal-app: C++ Compiler: '${CMAKE_CXX_COMPILER}'")
message("app: build version '${PROJECT_VERSION}'")

# Dependencies
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3)

set(APP_HEADERS
    include/training-sim.h
    # Add other .h files here
)

set(APP_SOURCES
    source/metal-app.cpp
    source/training-sim.cpp
)

source_group("Header Files" FILES ${APP_HEADERS})
source_group("Source Files" FILES ${APP_SOURCES})

add_executable(neural-lod-simulator
    ${APP_SOURCES}
    ${APP_HEADERS}
)

set_target_properties(neural-lod-simulator PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(neural-lod-simulator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/metal-renderer/include
    ${CMAKE_SOURCE_DIR}/metal-cpp
    ${CMAKE_SOURCE_DIR}/metal-cpp/tinyobjloader
    ${CMAKE_SOURCE_DIR}/metal-cpp/stb
)

# Linking
target_link_libraries(neural-lod-simulator 
    metal-cpp 
    "-framework Metal" 
    "-framework QuartzCore" 
    "-framework Foundation"
    metal-renderer
    SDL3::SDL3
)

add_custom_command(TARGET neural-lod-simulator POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_directory 
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/" 
    "$<TARGET_FILE_DIR:neural-lod-simulator>/assets"
    COMMENT "Copying training assets to build directory..."
)
