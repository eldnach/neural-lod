project(metal-cpp)

add_library(
${PROJECT_NAME} STATIC
metal-cpp.cpp
)

target_include_directories(${PROJECT_NAME}
PRIVATE ${PROJECT_SOURCE_DIR}
)