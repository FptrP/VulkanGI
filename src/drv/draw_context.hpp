#ifndef DRAW_CONTEXT_HPP
#define DRAW_CONTEXT_HPP

#include "context.hpp"
#include "resources.hpp"

namespace drv {

  const u32 MAX_FRAMES_IN_FLIGHT = 2;

  struct DrawContext {
    u32 frame_id;
    u32 image_id;
    vk::Framebuffer &backbuffer;
    vk::CommandBuffer dcb;
  };

  struct DrawContextPool {
    void init_backbuffer_views(Context &ctx);
    void init(Context &ctx, const std::vector<vk::Framebuffer> &fb);

    const std::vector<vk::ImageView> get_backbuffer_views() const {
      return backbuffer_images;
    }


    void init(Context &ctx, vk::RenderPass &pass);
    void init(Context &ctx, vk::RenderPass &pass, ResourceStorage &storage);

    void release(Context &ctx);

    DrawContext get_next(Context &ctx);
    void submit(Context &ctx, DrawContext &dctx);

  private:
    void create_sync_resources(Context &ctx);
    void create_depth_buffers(Context &ctx, ResourceStorage &storage);

    vk::CommandPool buffer_pool;
    std::vector<vk::ImageView> backbuffer_images;
    std::vector<vk::Framebuffer> backbuffers;
    std::vector<vk::CommandBuffer> cmd_buffers;
    
    std::vector<ImageViewID> backbuffer_depth;

    vk::Semaphore image_awailable[MAX_FRAMES_IN_FLIGHT];
    vk::Semaphore submit_done[MAX_FRAMES_IN_FLIGHT];
    vk::Fence frame_done[MAX_FRAMES_IN_FLIGHT];

    u32 frame_id = 0;
  };

  

}

#endif