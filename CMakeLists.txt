cmake_minimum_required(VERSION 3.16)

# One release artifact for every supported Mac. Setting these before project()
# makes our code and both source-built dependencies universal, while macOS 11
# is the oldest release that can run the arm64 slice.
if(CMAKE_HOST_APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS release architectures" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "oldest supported macOS" FORCE)
endif()

project(Boing VERSION 1.0.0 LANGUAGES C)
if(NOT APPLE)
    message(FATAL_ERROR "Boing supports macOS only")
endif()

set(CMAKE_C_STANDARD 99)

# Size is the hard constraint (<= 1.44MB), so default to a size-optimized build
# and strip the binary. `build/` alone now produces the shippable app bundle.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE MinSizeRel)
endif()

# 1. Add raylib from your local subdirectory
# This automatically configures raylib's own targets. AppleClang 21 warns on an
# upstream stb_vorbis pointer-wrap guard, so silence only that vendored warning
# without changing the submodule or weakening diagnostics on our target.
add_subdirectory(lib/raylib)
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(raylib PRIVATE -Wno-tautological-compare)
endif()

# nanosvg (zlib license) as its own tiny target. It is header-only and pinned as
# a git submodule at lib/nanosvg/, so the one translation unit defining its two
# *_IMPLEMENTATION macros lives beside the submodule rather than inside it
# (a submodule is upstream's tree — we don't add files to it).
#
# Kept off the Boing target so third-party code never sees our -Wall -Wextra or
# clang-tidy: 4750 lines of it would dominate every rebuild. Same section flags,
# so -dead_strip still drops the parser/rasterizer entry points we never call.
add_library(nanosvg STATIC lib/nanosvg_impl.c)
target_include_directories(nanosvg PUBLIC lib/nanosvg/src)
target_compile_options(nanosvg PRIVATE -ffunction-sections -fdata-sections)

# 2. Define the application bundle. raylib, GLFW, and nanosvg link statically;
# only Apple-provided system frameworks remain dynamic at runtime.
file(GLOB_RECURSE SOURCES "src/*.c")
set(BUNDLE_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/Boing.icns"
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/raylib/LICENSE"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/nanosvg/LICENSE.txt"
)
add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SOURCES})
# CMake's generated MACOSX_PACKAGE_LOCATION copies run after TARGET POST_BUILD
# commands under Unix Makefiles, which would sign and size-check an incomplete
# bundle. Copy explicitly before linking instead; LINK_DEPENDS makes a changed
# resource relink, recopy, resign, and recheck the app.
set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS
    ${BUNDLE_RESOURCES})
add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/Licenses/Boing"
        "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/Licenses/raylib"
        "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/Licenses/nanosvg"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/Boing.icns"
        "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/Boing.icns"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/Licenses/Boing/LICENSE"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/raylib/LICENSE"
        "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/Licenses/raylib/LICENSE"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/nanosvg/LICENSE.txt"
        "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/Licenses/nanosvg/LICENSE.txt"
    COMMENT "Copying bundle resources before signing"
    VERBATIM
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "Boing"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.hiyabye.boing"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "Copyright 2026 Jihoon Lee"
)

# 3. Include header search paths
target_include_directories(${PROJECT_NAME} PRIVATE
    include
    lib/raylib/src
)

# Drop unreferenced code/data at link time (removes raylib's unused audio/image/
# font-format decoders the game never calls). Compile with per-function/data
# sections so macOS ld64 can dead-strip at that granularity.
# -Wall -Wextra on our target only (not the raylib submodule). Deliberately
# without -Wconversion: the code leans on raylib's int APIs, so implicit
# float->int narrowing is expected and would flood the build (see .clang-tidy).
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -ffunction-sections -fdata-sections)
target_link_options(${PROJECT_NAME} PRIVATE -Wl,-dead_strip)

# 4. Link static third-party code and macOS system frameworks.
target_link_libraries(${PROJECT_NAME} PRIVATE
    raylib
    nanosvg
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreVideo"
    "-framework OpenGL"
)

# Strip symbols after linking so the measured size is the shipped size (but keep
# them for a Debug override so it stays debuggable).
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:${PROJECT_NAME}>)
endif()

# A local ad-hoc signature seals the complete bundle after stripping. It is not
# a substitute for Developer ID; release signing replaces it before notarizing.
# Check both CPU slices on every build so a thin release cannot happen silently.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND /usr/bin/lipo $<TARGET_FILE:${PROJECT_NAME}> -verify_arch arm64 x86_64
    COMMAND /usr/bin/codesign --force --options runtime --sign - $<TARGET_BUNDLE_DIR:${PROJECT_NAME}>
    COMMAND /usr/bin/codesign --verify --deep --strict $<TARGET_BUNDLE_DIR:${PROJECT_NAME}>
    VERBATIM
)

# Debug symbols intentionally exceed the shipping budget. Release builds check
# the contest's decompressed whole-bundle limit after resources and signing.
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DBUNDLE=$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>
            -DLIMIT=1474560
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckBundleSize.cmake
        VERBATIM
    )
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
