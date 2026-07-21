cmake_minimum_required(VERSION 3.16)
project(Boing C)

set(CMAKE_C_STANDARD 99)

# Size is the hard constraint (<= 1.44MB), so default to a size-optimized build
# and strip the binary. `build/` alone now produces the shippable executable.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE MinSizeRel)
endif()

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

# Strip symbols after linking so the measured size is the shipped size (but keep
# them for a Debug override so it stays debuggable).
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:${PROJECT_NAME}>)
endif()
