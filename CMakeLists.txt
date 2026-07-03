cmake_minimum_required(VERSION 3.10)
project(MusicIdleGame VERSION 1.0 LANGUAGES CXX)

# Enforce modern C++ (C++17 is a solid choice for modern features like std::filesystem)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# --- Dependencies ---
# Find OpenGL system libraries
find_package(OpenGL REQUIRED)

# Use pkg-config to find GLFW (Highly reliable inside Nix environments)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)

# --- ImGui Setup ---
# We assume you will clone/download ImGui into a "vendor/imgui" folder
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui)

# Helpful check to remind you to download ImGui before compiling
if(NOT EXISTS ${IMGUI_DIR}/imgui.cpp)
    message(WARNING "ImGui not found! Please download ImGui into ${IMGUI_DIR} before building.")
else()
    set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )
endif()

# --- Main Executable ---
# Automatically grab all .cpp files you add to your src/ folder
file(GLOB_RECURSE APP_SOURCES "src/*.cpp")

# Define the final application binary
add_executable(${PROJECT_NAME} 
    ${APP_SOURCES}
    ${IMGUI_SOURCES}
)

# --- Includes and Linking ---
# Tell the compiler where to look for header files (.h)
target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${GLFW_INCLUDE_DIRS}
)

# Link the required libraries to the binary
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    ${GLFW_LIBRARIES}
    dl          # Dynamic linking loader
    pthread     # POSIX threads
    util        # REQUIRED FOR openpty()
)