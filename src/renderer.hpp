#ifndef RENDERER_HPP_INCLUDED
#define RENDERER_HPP_INCLUDED

#include <thread>
#include <atomic>

#include "driverstate.hpp"
#include "triangle.hpp"
#include "shading.hpp"

enum class RenderEvents {
  None = 0,
  Finish = 1,
  ReloadShaders = 2,
};

struct Renderer {
  void init(SDL_Window *w);
  void release();

  vk::RenderPass create_main_renderpass();
  void update(float dt);
  void handle_event(const SDL_Event &event);

  void render(drv::DrawContext &ctx);
  void main_loop();

private:
  void create_framebuffers(std::vector<vk::Framebuffer> &fb);

  DriverState ds;

  SDL_Window *window = nullptr;

  std::atomic<u32> events_msk;
  FrameGlobal frame_data;
  GBufferSubpass *gbuffer_subpass = nullptr;
  ShadingPass *shading_subpass = nullptr;
};

#endif