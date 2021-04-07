#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>

#include <SDL2/SDL.h>
#include "drv/context.hpp"
#include "drv/draw_context.hpp"
#include "drv/pipeline.hpp"
#include "drv/resources.hpp"
#include "drv/descriptors.hpp"

#include "renderer.hpp"

int main() {
  SDL_Init(SDL_INIT_EVERYTHING);

  auto window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080, SDL_WINDOW_VULKAN);
  
  Renderer renderer{};
  renderer.init(window);
  renderer.main_loop();

  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}