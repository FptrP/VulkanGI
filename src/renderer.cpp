#include "renderer.hpp"

void Renderer::init(SDL_Window *w) {
  window = w;
  
  ds.ctx.init(window);
  ds.storage.init(ds.ctx);

  ds.main_renderpass = create_main_renderpass();
  ds.submit_pool.init(ds.ctx, ds.main_renderpass);
  
  ds.pipelines.load_shader(ds.ctx, "pass_vs", "src/shaders/pass_vert.spv", vk::ShaderStageFlagBits::eVertex);
  ds.pipelines.load_shader(ds.ctx, "shading_fs", "src/shaders/shading_frag.spv", vk::ShaderStageFlagBits::eFragment);

  frame_data = new FrameGlobal{};
  frame_data->init(ds);

  gbuffer_subpass = new GBufferSubpass{*frame_data};
  gbuffer_subpass->init(ds);

  shading_subpass = new ShadingPass{ds, *frame_data};
}

void Renderer::release() {
  shading_subpass->release(ds);
  delete shading_subpass;

  gbuffer_subpass->release(ds);
  delete gbuffer_subpass;

  ds.submit_pool.release(ds.ctx);
  frame_data->release(ds);
  delete frame_data;
  ds.pipelines.release(ds.ctx);
  ds.storage.release(ds.ctx);
  ds.ctx.get_device().destroyRenderPass(ds.main_renderpass);
}

vk::RenderPass Renderer::create_main_renderpass() {

  vk::AttachmentDescription backbuf_desc{};
  backbuf_desc
  .setInitialLayout(vk::ImageLayout::eUndefined)
  .setFinalLayout(vk::ImageLayout::ePresentSrcKHR)
  .setFormat(ds.ctx.get_swapchain_fmt())
  .setLoadOp(vk::AttachmentLoadOp::eDontCare)
  .setStoreOp(vk::AttachmentStoreOp::eStore)
  .setSamples(vk::SampleCountFlagBits::e1)
  .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
  .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);

  auto attachments = {backbuf_desc};

  vk::AttachmentReference out_color {};
  out_color
    .setLayout(vk::ImageLayout::eColorAttachmentOptimal)
    .setAttachment(0);

  auto out_attachments = {out_color};

  vk::SubpassDescription shading_pass {};
  shading_pass
    .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
    .setColorAttachments(out_attachments);

  auto subpasses = {shading_pass};

  vk::SubpassDependency dep {};
  dep
    .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite|vk::AccessFlagBits::eDepthStencilAttachmentWrite)
    .setSrcStageMask(vk::PipelineStageFlagBits::eAllGraphics)
    .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
    .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
    .setSrcSubpass(VK_SUBPASS_EXTERNAL)
    .setDstSubpass(0);

  auto dependencies = {dep};

  vk::RenderPassCreateInfo info {};
  info
    .setDependencies(dependencies)
    .setAttachments(attachments)
    .setSubpasses(subpasses);

  return ds.ctx.get_device().createRenderPass(info);
}

void Renderer::create_framebuffers(std::vector<vk::Framebuffer> &fb) {
  auto &images = ds.submit_pool.get_backbuffer_views();
  auto &gbuf = frame_data->get_gbuffer();
  auto ext = ds.ctx.get_swapchain_extent();

  for (u32 i = 0; i < images.size(); i++) {
    
    auto attachments = {gbuf.images[0]->api_view(), gbuf.images[1]->api_view(), gbuf.images[2]->api_view(), gbuf.images[3]->api_view(), images[i]};
    
    vk::FramebufferCreateInfo info {};
    info
      .setWidth(ext.width)
      .setHeight(ext.height)
      .setLayers(1)
      .setRenderPass(ds.main_renderpass)
      .setAttachments(attachments);
    
    fb.push_back(ds.ctx.get_device().createFramebuffer(info));
  }
}

void Renderer::update(float dt) {
  frame_data->update(dt);
  gbuffer_subpass->update(dt);
}

void Renderer::handle_event(const SDL_Event &event) {
  if (event.type == SDL_QUIT) {
    events_msk.store((u32)RenderEvents::Finish, std::memory_order_relaxed);
  }

  if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r) {
    events_msk.store((u32)RenderEvents::ReloadShaders, std::memory_order_relaxed);
  }

  frame_data->handle_events(event);
}

void Renderer::render(drv::DrawContext &dctx) {
  gbuffer_subpass->render(dctx, ds);
  
  vk::ClearValue color {};
  color.color.setFloat32({0.f, 0.f, 0.f, 0.f});
        
  auto clear_vals = {color};

  vk::RenderPassBeginInfo info {};
  info.setRenderPass(ds.main_renderpass);
  info.setFramebuffer(dctx.backbuffer);
  info.setClearValues(clear_vals);
  info.setRenderArea(vk::Rect2D{{0, 0}, ds.ctx.get_swapchain_extent()});
      
  dctx.dcb.beginRenderPass(info, vk::SubpassContents::eInline);

  shading_subpass->render(dctx, ds);
  
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