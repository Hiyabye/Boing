cmake_minimum_required(VERSION 3.16)
project(Megajump C)

set(CMAKE_C_STANDARD 99)

# 1. Add raylib from your local subdirectory
# This automatically configures raylib's own targets
add_subdirectory(lib/raylib)

# 2. Define your executable targets
file(GLOB_RECURSE SOURCES "src/*.c")
add_executable(${PROJECT_NAME} ${SOURCES})

# 3. Include header search paths
target_include_directories(${PROJECT_NAME} PRIVATE
    include
    lib/raylib/src
)

# 4. Link raylib and macOS-specific system frameworks
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework OpenGL"
    )
endif()
