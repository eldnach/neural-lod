cmake_minimum_required(VERSION 3.21)
message("using cmake version: '" ${CMAKE_VERSION} "'")
set(CMAKE_CXX_STANDARD 17)
project(neural-lods)

add_subdirectory(metal-cpp)  
add_subdirectory(metal-renderer)
add_subdirectory(metal-app)

message("processing 'CMakeLists.txt' and configuring 'CMakeCache.txt'")
message("generating native solution at '" ${CMAKE_BINARY_DIR} "'")
