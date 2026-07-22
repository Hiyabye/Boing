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

# Drop unreferenced code/data at link time (removes raylib's unused audio/image/
# font-format decoders the game never calls). macOS ld64 uses -dead_strip;
# GNU ld uses --gc-sections. Compile with per-function/data sections so the
# linker can strip at that granularity.
target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections)
if(APPLE)
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,-dead_strip)
else()
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--gc-sections)
endif()

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

# clang-tidy on every build — only our target, never the raylib submodule
# (setting the property on the target instead of CMAKE_C_CLANG_TIDY globally).
# Config comes from .clang-tidy.
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    set_target_properties(${PROJECT_NAME} PROPERTIES C_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()

# `cmake --build build --target format` reformats our sources in place (.clang-format).
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE FORMAT_SOURCES "src/*.c" "include/*.h")
    add_custom_target(format COMMAND ${CLANG_FORMAT_EXE} -i ${FORMAT_SOURCES})
endif()
