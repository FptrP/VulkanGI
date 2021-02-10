#include "draw_context.hpp"
#include <iostream>
namespace drv {

  void DrawContextPool::init_backbuffer_views(Context &ctx) {
    auto &images = ctx.get_swapchain_images();

    for (auto &img : images) {
      vk::ImageViewCreateInfo info {};
      info.setFormat(ctx.get_swapchain_fmt());
      info.setImage(img);
      info.setViewType(vk::ImageViewType::e2D);
      
      vk::ImageSubresourceRange range {};
      range.setLayerCount(1);
      range.setLevelCount(1);
      range.setBaseMipLevel(0);
      range.setBaseArrayLayer(0);
      range.setAspectMask(vk::ImageAspectFlagBits::eColor);

      info.setSubresourceRange(range);
      
      backbuffer_images.push_back(ctx.get_device().createImageView(info));
    }
  }

  void DrawContextPool::init(Context &ctx, const std::vector<vk::Framebuffer> &fb) {
    if (fb.size() != backbuffer_images.size()) {
      throw std::runtime_error {"Not enougth framebuffers"};
    }
  
    backbuffers = fb;
    create_sync_resources(ctx);
  }

  void DrawContextPool::init(Context &ctx, vk::RenderPass &pass) {
    init_backbuffer_views(ctx);

    auto ext = ctx.get_swapchain_extent();
    auto has_depth = !backbuffer_depth.empty();

    for (u32 i = 0; i < backbuffer_images.size(); i++) {
      vk::FramebufferCreateInfo info {};
      info.setWidth(ext.width);
      info.setHeight(ext.height);
      if (has_depth) {
        auto attachments = {backbuffer_images[i], backbuffer_depth[i]->api_view()};
        info.setAttachments(attachments);
      } else {
        auto attachments = {backbuffer_images[i]};
        info.setAttachments(attachments);
      }
      
      info.setLayers(1);
      info.setRenderPass(pass);

      backbuffers.push_back(ctx.get_device().createFramebuffer(info));
    }

    create_sync_resources(ctx);
  }

  void DrawContextPool::create_sync_resources(Context &ctx) {
    {
      vk::CommandPoolCreateInfo info {};
      info.setQueueFamilyIndex(ctx.queue_index(QueueT::Graphics));
      info.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient);
      buffer_pool = ctx.get_device().createCommandPool(info);
    }

    {
      vk::CommandBufferAllocateInfo info {};
      info.setCommandBufferCount(MAX_FRAMES_IN_FLIGHT);
      info.setCommandPool(buffer_pool);
      info.setLevel(vk::CommandBufferLevel::ePrimary);
      cmd_buffers = ctx.get_device().allocateCommandBuffers(info);  
    }

    {
      vk::SemaphoreCreateInfo info {};
      vk::FenceCreateInfo fence {};
      fence.setFlags(vk::FenceCreateFlagBits::eSignaled);

      for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
      {
        image_awailable[i] = ctx.get_device().createSemaphore(info);
        submit_done[i] = ctx.get_device().createSemaphore(info);
        frame_done[i] = ctx.get_device().createFence(fence);
      }  
    }
  }

  void DrawContextPool::init(Context &ctx, vk::RenderPass &pass, ResourceStorage &storage) {
    create_depth_buffers(ctx, storage);
    init(ctx, pass);
  }

  void DrawContextPool::create_depth_buffers(Context &ctx, ResourceStorage &storage) {
    const u32 buff_count = ctx.get_swapchain_images().size();
    auto extent = ctx.get_swapchain_extent();

    for (u32 i = 0; i < buff_count; i++) {
      auto img = storage.create_depth2D_rt(ctx, extent.width, extent.height);
      auto view = storage.create_rt_view(ctx, img, vk::ImageAspectFlagBits::eDepth);
      backbuffer_depth.push_back(view);
    }
  }
  
  void DrawContextPool::release(Context &ctx) {
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      ctx.get_device().destroyFence(frame_done[i]);
      ctx.get_device().destroySemaphore(image_awailable[i]);
      ctx.get_device().destroySemaphore(submit_done[i]);
    }

    ctx.get_device().destroyCommandPool(buffer_pool);

    for (u32 i = 0; i < backbuffer_images.size(); i++) {
      ctx.get_device().destroyFramebuffer(backbuffers[i]);
      ctx.get_device().destroyImageView(backbuffer_images[i]);
      
      if (!backbuffer_depth.empty()) {
        backbuffer_depth[i].release();
      }
    }

  }

  DrawContext DrawContextPool::get_next(Context &ctx) {
    auto wait_fences = {frame_done[frame_id]};
    ctx.get_device().waitForFences(wait_fences, 1, UINT64_MAX);
    ctx.get_device().resetFences(wait_fences);

    u32 image_id = ctx.get_device().acquireNextImageKHR(ctx.get_swapchain(), UINT64_MAX, image_awailable[frame_id], nullptr);

    cmd_buffers[frame_id].reset(vk::CommandBufferResetFlagBits::eReleaseResources);
    
    DrawContext dctx {
      frame_id,
      image_id,
      backbuffers[image_id],
      cmd_buffers[frame_id]
    };

    vk::CommandBufferBeginInfo info {};
    dctx.dcb.begin(info);
    return dctx;
  }

  void DrawContextPool::submit(Context &ctx, DrawContext &dctx) {
    assert(((dctx.frame_id == frame_id) && "submit order mismatch"));

    auto wait_sem = {image_awailable[frame_id]};
    vk::PipelineStageFlags wait_msk[] {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    auto signal_sem = {submit_done[frame_id]};
    auto submit_buffers = { dctx.dcb };
    
    vk::SubmitInfo info {};
    info.setWaitSemaphores(wait_sem);
    info.setPWaitDstStageMask(wait_msk);
    info.setSignalSemaphores(signal_sem);
    info.setCommandBuffers(submit_buffers);
    
    ctx.get_queue(QueueT::Graphics).submit(info, frame_done[frame_id]);
    
    auto swapchains = { ctx.get_swapchain() };
    auto images = { dctx.image_id };
    vk::PresentInfoKHR pres {};
    pres.setWaitSemaphores(signal_sem);
    pres.setSwapchains(swapchains);
    pres.setImageIndices(images);
    ctx.get_queue(QueueT::Graphics).presentKHR(pres);

    frame_id = (frame_id + 1) % MAX_FRAMES_IN_FLIGHT;
  }

  vk::CommandBuffer DrawContextPool::start_cmd(Context &ctx) {
    
    vk::CommandBufferAllocateInfo info {};
    info
      .setCommandBufferCount(1)
      .setCommandPool(buffer_pool)
      .setLevel(vk::CommandBufferLevel::ePrimary);
    
    auto buffers = ctx.get_device().allocateCommandBuffers(info);
    return buffers[0];
  }

  vk::Fence DrawContextPool::submit_cmd(Context &ctx, vk::CommandBuffer cmd) {
    auto buffers = {cmd};
    
    vk::SubmitInfo info {};
    info.setCommandBuffers(buffers);

    vk::FenceCreateInfo fence_info {};
    
    auto fence = ctx.get_device().createFence(fence_info);
    ctx.get_queue(QueueT::Graphics).submit(info, fence);
    return fence;
  }

  void DrawContextPool::free_cmd(Context &ctx, vk::CommandBuffer cmd) {
    ctx.get_device().freeCommandBuffers(buffer_pool, {cmd});
    //ctx.get_device().trimCommandPool(buffer_pool, vk::CommandPoolTrimFlags(0));
  }
}