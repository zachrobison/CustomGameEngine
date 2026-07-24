cmake_minimum_required(VERSION 3.20)
project(VoxelEngine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Statically link the MSVC C runtime on Windows so the shipped .exe needs no
# "Visual C++ Redistributable" install on the player's machine.
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)

# ── Dear ImGui ────────────────────────────────────────────────────────────────
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.90.4
)
FetchContent_MakeAvailable(imgui)

# ── GLFW + GLM built from source and baked into the binary ────────────────────
# This is what lets players just download & run: no glfw/glm to install, and no
# glfw DLL to ship — it's compiled straight into the executable.
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.4
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw)

# ── Engine executable ─────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES src/*.cpp)
if(APPLE)
  file(GLOB_RECURSE MM_SOURCES src/*.mm)   # Objective-C++ (native macOS bits)
  list(APPEND SOURCES ${MM_SOURCES})
endif()

add_executable(VoxelEngine ${SOURCES})
target_include_directories(VoxelEngine PRIVATE src)
target_link_libraries(VoxelEngine imgui_lib glfw glm::glm)
# MSVC needs this for M_PI et al. from <cmath>; harmless elsewhere.
target_compile_definitions(VoxelEngine PRIVATE _USE_MATH_DEFINES)

if(APPLE)
  set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/src/platform/FileDialog.mm
    PROPERTIES COMPILE_FLAGS "-x objective-c++"
  )
  target_link_libraries(VoxelEngine
    "-framework OpenGL"
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreVideo"
  )
else()
  # Windows / Linux: modern GL needs a loader (GLEW) + the system GL lib.
  find_package(OpenGL REQUIRED)
  find_package(GLEW   REQUIRED)
  target_link_libraries(VoxelEngine GLEW::GLEW OpenGL::GL)
  if(WIN32)
    target_compile_definitions(VoxelEngine PRIVATE GLEW_STATIC)  # single-file .exe
  endif()
endif()

# Copy shaders and assets next to the executable on each build
add_custom_command(TARGET VoxelEngine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:VoxelEngine>/shaders
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:VoxelEngine>/assets
  COMMENT "Copying shaders and assets"
)
