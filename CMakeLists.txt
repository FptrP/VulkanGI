cmake_minimum_required(VERSION 2.8)

project(main)

find_package(SDL2 REQUIRED)

find_package(assimp REQUIRED)

include_directories(${SDL2_INCLUDE_DIRS})
message("SDLInclude: ${SDL2_INCLUDE_DIRS}")
message("Assimp libraries: ${ASSIMP_LIBRARIES}")
include_directories(src)
#include_directories(shaders)
#include_directories(src/imgui)
#include_directories(src/tinygltf)

find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
message(${Vulkan_INCLUDE_DIRS})
message(${Vulkan_LIBRARIES})


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall -g")

#file(GLOB IMGUI_SRC src/imgui/*.cpp)

set(SOURCES 
  src/main.cpp
  src/renderer.cpp
  src/scene.cpp
  src/drv/context.cpp
  src/drv/draw_context.cpp
  src/drv/pipeline.cpp
  src/drv/memory.cpp
  src/drv/buffers.cpp
  src/drv/pipeline_layout.cpp
  src/drv/descriptors.cpp
  src/drv/images.cpp
)

add_executable(main ${SOURCES})
target_link_libraries(main ${SDL2_LIBRARIES} ${Vulkan_LIBRARIES} assimp pthread)
