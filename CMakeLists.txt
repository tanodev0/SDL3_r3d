cmake_minimum_required(VERSION 3.16)

project(SDL3_r3d
    VERSION 0.1.0
    DESCRIPTION "Software 3D drawing on top of SDL_Renderer"
    LANGUAGES C)

option(SDLR3D_SHARED    "Build a shared library (otherwise static)" ON)
option(SDLR3D_EXAMPLES  "Build the examples" ON)
option(SDLR3D_TESTS     "Build the tests" ON)
option(SDLR3D_FETCH_SDL "Download and build SDL3 if it is not installed" ON)
set(SDLR3D_SDL_TAG "release-3.4.20" CACHE STRING "SDL3 git tag used when SDL3 is downloaded")

# On Windows, put every .exe and .dll in one folder so the examples and tests
# find SDL3.dll and SDL3_r3d.dll when they run. Only when we are the main
# project: a parent project decides its own layout.
if(WIN32 AND CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

# Use an installed SDL3 if there is one; otherwise download and build it.
find_package(SDL3 CONFIG QUIET)
set(sdlr3d_sdl_vendored OFF)
if(NOT SDL3_FOUND)
    if(NOT SDLR3D_FETCH_SDL)
        message(FATAL_ERROR "SDL3 not found. Install SDL3, set CMAKE_PREFIX_PATH, or turn on SDLR3D_FETCH_SDL.")
    endif()
    message(STATUS "SDL3 not found: downloading ${SDLR3D_SDL_TAG}")
    include(FetchContent)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        ${SDLR3D_SDL_TAG}
        GIT_SHALLOW    TRUE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3)
    set(sdlr3d_sdl_vendored ON)
endif()

# ---- Library ---------------------------------------------------------------

if(SDLR3D_SHARED)
    set(sdlr3d_type SHARED)
else()
    set(sdlr3d_type STATIC)
endif()

add_library(SDL3_r3d ${sdlr3d_type}
    src/SDL_r3d.c
    src/SDL_r3d_math.c
    src/SDL_r3d_camera.c
    src/SDL_r3d_draw.c
    src/SDL_r3d_mesh.c
)
add_library(SDL3_r3d::SDL3_r3d ALIAS SDL3_r3d)

target_include_directories(SDL3_r3d PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_link_libraries(SDL3_r3d PUBLIC SDL3::SDL3)

# The math layer uses the C math library (sqrtf, sinf, ...). On Linux and
# other Unix systems it lives in a separate library, libm; on macOS and
# Windows it is part of the C runtime.
find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    target_link_libraries(SDL3_r3d PRIVATE ${MATH_LIBRARY})
endif()

set_target_properties(SDL3_r3d PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_VISIBILITY_PRESET hidden          # only SDL_DECLSPEC functions are exported
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})

# Windows DLLs export only what is marked __declspec(dllexport). SDL's
# SDL_DECLSPEC becomes exactly that when DLL_EXPORT is defined (SDL_ttf and
# SDL_image do the same).
if(WIN32 AND SDLR3D_SHARED)
    target_compile_definitions(SDL3_r3d PRIVATE DLL_EXPORT)
endif()

if(MSVC)
    target_compile_options(SDL3_r3d PRIVATE /W3)
else()
    target_compile_options(SDL3_r3d PRIVATE -Wall -Wextra)
endif()

# ---- Install (so other projects can use find_package(SDL3_r3d)) -----------
# Only with an installed SDL3: a downloaded SDL3 is not part of our package.

if(NOT sdlr3d_sdl_vendored)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(sdlr3d_cmake_dir ${CMAKE_INSTALL_LIBDIR}/cmake/SDL3_r3d)

install(TARGETS SDL3_r3d EXPORT SDL3_r3dTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/SDL3_r3d DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT SDL3_r3dTargets
    NAMESPACE SDL3_r3d::
    DESTINATION ${sdlr3d_cmake_dir})

configure_package_config_file(cmake/SDL3_r3dConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SDL3_r3dConfig.cmake
    INSTALL_DESTINATION ${sdlr3d_cmake_dir})
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/SDL3_r3dConfigVersion.cmake
    COMPATIBILITY SameMinorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/SDL3_r3dConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/SDL3_r3dConfigVersion.cmake
    DESTINATION ${sdlr3d_cmake_dir})
endif()

# ---- Examples --------------------------------------------------------------

if(SDLR3D_EXAMPLES)
    foreach(e showaxes meshes plot solarsystem custommesh cubes)
        add_executable(${e} examples/${e}.c)
        target_link_libraries(${e} PRIVATE SDL3_r3d::SDL3_r3d)
    endforeach()
endif()

# ---- Tests -----------------------------------------------------------------

if(SDLR3D_TESTS)
    enable_testing()
    foreach(t testmath testrender)
        add_executable(${t} test/${t}.c)
        target_link_libraries(${t} PRIVATE SDL3_r3d::SDL3_r3d)
        add_test(NAME ${t} COMMAND ${t})
    endforeach()
endif()
