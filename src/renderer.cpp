#include "renderer.hpp"

void Renderer::init(SDL_Window *w) {
  window = w;
  
  ds.ctx.init(window);
  ds.storage.init(ds.ctx);

  ds.main_renderpass = create_main_renderpass();
  ds.submit_pool.init(ds.ctx, ds.main_renderpass, ds.storage);
  
  
  gbuffer_subpass = new GBufferSubpass{frame_data};
  gbuffer_subpass->init(ds);
}

void Renderer::release() {
  gbuffer_subpass->release(ds);
  delete gbuffer_subpass;

  ds.submit_pool.release(ds.ctx);
  ds.pipelines.release(ds.ctx);
  ds.storage.release(ds.ctx);
  ds.ctx.get_device().destroyRenderPass(ds.main_renderpass);
}

vk::RenderPass Renderer::create_main_renderpass() {
  vk::AttachmentDescription desc {};
  
  desc.setInitialLayout(vk::ImageLayout::eUndefined);
  desc.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);
  desc.setFormat(ds.ctx.get_swapchain_fmt());
  desc.setLoadOp(vk::AttachmentLoadOp::eClear);
  desc.setStoreOp(vk::AttachmentStoreOp::eStore);
  desc.setSamples(vk::SampleCountFlagBits::e1);
  
  vk::AttachmentDescription depth {};
  depth
    .setInitialLayout(vk::ImageLayout::eUndefined)
    .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
    .setFormat(vk::Format::eD24UnormS8Uint)
    .setLoadOp(vk::AttachmentLoadOp::eClear)
    .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
    .setSamples(vk::SampleCountFlagBits::e1)
    .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
    .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);

  vk::AttachmentReference ref {};
  ref.setAttachment(0);
  ref.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

  vk::AttachmentReference depth_ref{};
  depth_ref.setAttachment(1);
  depth_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

  auto attachments = {desc, depth};
  auto output = {ref};

  vk::SubpassDescription subpass {};
  subpass.setColorAttachments(output);
  subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
  subpass.setPDepthStencilAttachment(&depth_ref);

  vk::SubpassDependency dep {};
  dep.setSrcSubpass(VK_SUBPASS_EXTERNAL);
  dep.dstSubpass = 0;
  dep.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput|vk::PipelineStageFlagBits::eEarlyFragmentTests);
  dep.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput|vk::PipelineStageFlagBits::eEarlyFragmentTests);
  dep.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite|vk::AccessFlagBits::eDepthStencilAttachmentWrite);

  auto dependencies = {dep};
  vk::RenderPassCreateInfo info {};

  info.setAttachments(attachments);
  info.setSubpasses(subpass);
  info.setDependencies(dependencies);
  return ds.ctx.get_device().createRenderPass(info);
}

void Renderer::update(float dt) {
  frame_data.update(dt);
  gbuffer_subpass->update(dt);
}

void Renderer::handle_event(const SDL_Event &event) {
  if (event.type == SDL_QUIT) {
    events_msk.store((u32)RenderEvents::Finish, std::memory_order_relaxed);
  }

  if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r) {
    events_msk.store((u32)RenderEvents::ReloadShaders, std::memory_order_relaxed);
  }

  frame_data.handle_events(event);
}

void Renderer::render(drv::DrawContext &dctx) {
  vk::ClearValue color {};
  color.color.setFloat32({0.f, 0.f, 0.f, 0.f});
        
  vk::ClearValue depth_clear {};
  depth_clear.depthStencil.setDepth(1.f);
  auto clear_vals = {color, depth_clear};

  vk::RenderPassBeginInfo info {};
  info.setRenderPass(ds.main_renderpass);
  info.setFramebuffer(dctx.backbuffer);
  info.setClearValues(clear_vals);
  info.setRenderArea(vk::Rect2D{{0, 0}, ds.ctx.get_swapchain_extent()});
      
  dctx.dcb.beginRenderPass(info, vk::SubpassContents::eInline);

  gbuffer_subpass->render(dctx, ds);

  dctx.dcb.endRenderPass();
  dctx.dcb.end();
}

void Renderer::main_loop() {

  std::thread update_thread([this](){
    bool quit = false;
    auto start = SDL_GetTicks();
    while (!quit) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        this->handle_event(event);
        quit = quit || (event.type == SDL_QUIT);
      }
      SDL_Delay(15);
      
      auto now = SDL_GetTicks();
      float dt = (now - start)/1000.f;
      start = now;
      this->update(dt);
    }
  });

  bool stop = false; 
  do {
    auto draw_ctx = ds.submit_pool.get_next(ds.ctx);
    render(draw_ctx);
    ds.submit_pool.submit(ds.ctx, draw_ctx);

    auto flags = events_msk.exchange(0, std::memory_order_relaxed);
    if (flags & (u32)RenderEvents::Finish) {
      stop = true;      
    }

    if (flags & (u32)RenderEvents::ReloadShaders) {
      ds.ctx.get_queue(drv::QueueT::Graphics).waitIdle();
      ds.pipelines.reload_shaders(ds.ctx);
    }

  } while(!stop);
  
  ds.ctx.get_device().waitIdle();
  update_thread.join();
  release();
}