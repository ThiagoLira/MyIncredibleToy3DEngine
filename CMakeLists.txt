cmake_minimum_required(VERSION 3.25)
project(toy3d LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # compile_commands.json for clangd / editors

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)          # Debug = validation layers on (see main.cpp)
endif()

# We need the Vulkan *headers* only. We deliberately do NOT link Vulkan::Vulkan:
# the loader library is opened at runtime by src/vk_loader.cpp (DESIGN.md §4).
find_package(Vulkan REQUIRED)

add_executable(toy3d
  src/main.cpp
  src/vk_loader.cpp
)
target_include_directories(toy3d PRIVATE src)
target_link_libraries(toy3d PRIVATE Vulkan::Headers ${CMAKE_DL_LIBS})
target_compile_options(toy3d PRIVATE -Wall -Wextra -Wpedantic -Werror)
