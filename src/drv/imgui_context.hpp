#ifndef IMGUI_CONTEXT_HPP_INCLUDED
#define IMGUI_CONTEXT_HPP_INCLUDED

#include "context.hpp"
#include "draw_context.hpp"

#include "lib/vkimgui/imgui.h"

namespace drv {
  
  struct ImguiContext {
    void init(Context &ctx, vk::RenderPass &renderpass, u32 subpass);
    void release(Context &ctx);
    void new_frame();
    void render(vk::CommandBuffer &cmd);
    void process_event(const SDL_Event &event);

    void create_fonts(Context &ctx, DrawContextPool &ctx_pool);
  private:
    vk::DescriptorPool pool;
    SDL_Window *window = nullptr;
  };


} 


#endif