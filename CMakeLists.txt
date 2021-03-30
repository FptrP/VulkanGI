cmake_minimum_required(VERSION 2.8)

project(main)

find_package(SDL2 REQUIRED)

find_package(assimp REQUIRED)

include_directories(${SDL2_INCLUDE_DIRS})
message("SDLInclude: ${SDL2_INCLUDE_DIRS}")
message("Assimp libraries: ${ASSIMP_LIBRARIES}")
include_directories(src)
include_directories(src/drv/lib/vkimgui)
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

set(IMGUI_SRC 
  src/drv/lib/vkimgui/imgui_demo.cpp
  src/drv/lib/vkimgui/imgui_draw.cpp
  src/drv/lib/vkimgui/imgui_impl_sdl.cpp
  src/drv/lib/vkimgui/imgui_impl_vulkan.cpp
  src/drv/lib/vkimgui/imgui_tables.cpp 
  src/drv/lib/vkimgui/imgui_widgets.cpp
  src/drv/lib/vkimgui/imgui.cpp
)

set(SOURCES 
  src/main.cpp
  src/renderer.cpp
  src/scene.cpp
  src/gbufferpass.cpp
  src/cubemap_shadow.cpp
  src/light_field_probes.cpp
  src/drv/context.cpp
  src/drv/draw_context.cpp
  src/drv/pipeline.cpp
  src/drv/memory.cpp
  src/drv/buffers.cpp
  src/drv/pipeline_layout.cpp
  src/drv/descriptors.cpp
  src/drv/images.cpp
  src/drv/cmd_utils.cpp
  src/drv/imgui_context.cpp
)

add_executable(main ${SOURCES} ${IMGUI_SRC})
target_link_libraries(main ${SDL2_LIBRARIES} ${Vulkan_LIBRARIES} assimp pthread)
