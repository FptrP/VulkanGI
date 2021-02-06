#include "renderer.hpp"

void Renderer::init(SDL_Window *w) {
  window = w;
  
  ds.ctx.init(window);
  ds.storage.init(ds.ctx);

  ds.main_renderpass = create_main_renderpass();
  frame_data.init(ds);

  std::vector<vk::Framebuffer> fb;
  ds.submit_pool.init_backbuffer_views(ds.ctx);
  create_framebuffers(fb);
  ds.submit_pool.init(ds.ctx, fb);
  
  
  gbuffer_subpass = new GBufferSubpass{frame_data};
  gbuffer_subpass->init(ds);

  shading_subpass = new ShadingPass{ds, frame_data};
}

void Renderer::release() {
  shading_subpass->release(ds);
  delete shading_subpass;

  gbuffer_subpass->release(ds);
  delete gbuffer_subpass;

  ds.submit_pool.release(ds.ctx);
  frame_data.release(ds);
  ds.pipelines.release(ds.ctx);
  ds.storage.release(ds.ctx);
  ds.ctx.get_device().destroyRenderPass(ds.main_renderpass);
}

vk::RenderPass Renderer::create_main_renderpass() {
  /*vk::AttachmentDescription desc {};
  
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
  return ds.ctx.get_device().createRenderPass(info);*/

  vk::AttachmentDescription albedo_desc {};
  albedo_desc
    .setInitialLayout(vk::ImageLayout::eUndefined)
    .setFinalLayout(vk::ImageLayout::eUndefined)
    .setFormat(vk::Format::eR8G8B8A8Srgb)
    .setLoadOp(vk::AttachmentLoadOp::eClear)
    .setStoreOp(vk::AttachmentStoreOp::eDontCare)
    .setSamples(vk::SampleCountFlagBits::e1)
    .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
    .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);

  vk::AttachmentDescription normal_desc = albedo_desc;
  vk::AttachmentDescription depth_desc = albedo_desc;
  depth_desc.setFormat(vk::Format::eD24UnormS8Uint);
  
  vk::AttachmentDescription backbuf_desc = albedo_desc;
  backbuf_desc
  .setLoadOp(vk::AttachmentLoadOp::eDontCare)
  .setStoreOp(vk::AttachmentStoreOp::eStore)
  .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

  auto attachments = {albedo_desc, normal_desc, depth_desc, backbuf_desc};

  vk::AttachmentReference gbuf_albedo {}, gbuf_normal {}, gbuf_depth {}, out_color {}, shading_albedo {}, shading_normal {}, shading_depth{};
  gbuf_albedo
    .setAttachment(0)
    .setLayout(vk::ImageLayout::eColorAttachmentOptimal);
  
  gbuf_normal
    .setAttachment(1)
    .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

  gbuf_depth
    .setAttachment(2)
    .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
  
  shading_albedo
    .setAttachment(0)
    .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
  
  shading_normal
    .setAttachment(1)
    .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

  shading_depth
    .setAttachment(2)
    .setLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal);

  out_color
    .setLayout(vk::ImageLayout::eColorAttachmentOptimal)
    .setAttachment(3);

  auto gbuf_attachments = {gbuf_albedo, gbuf_normal};
  auto gbuf_out = {shading_albedo, shading_normal, shading_depth};
  auto out_attachments = {out_color};

  vk::SubpassDescription gbuf_pass {};
  gbuf_pass
    .setColorAttachments(gbuf_attachments)
    .setPDepthStencilAttachment(&gbuf_depth)
    .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

  vk::SubpassDescription shading_pass {};
  shading_pass
    .setColorAttachments(out_color)
    .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
    .setInputAttachments(gbuf_out)
    .setColorAttachments(out_attachments);

  auto subpasses = {gbuf_pass, shading_pass};

  vk::SubpassDependency out_dep {};
  out_dep
    .setSrcSubpass(VK_SUBPASS_EXTERNAL)
    .setDstSubpass(0)
    .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput|vk::PipelineStageFlagBits::eEarlyFragmentTests)
    .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput|vk::PipelineStageFlagBits::eEarlyFragmentTests)
    .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite|vk::AccessFlagBits::eDepthStencilAttachmentWrite);

  vk::SubpassDependency gbuf_dep {};
  gbuf_dep
    .setSrcSubpass(0)
    .setDstSubpass(1)
    .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput|vk::PipelineStageFlagBits::eEarlyFragmentTests)
    .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
    .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite|vk::AccessFlagBits::eDepthStencilAttachmentWrite)
    .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eDepthStencilAttachmentRead);

  auto dep = {out_dep, gbuf_dep};

  vk::RenderPassCreateInfo info {};
  info
    .setAttachments(attachments)
    .setDependencies(dep)
    .setSubpasses(subpasses);

  return ds.ctx.get_device().createRenderPass(info);
}

void Renderer::create_framebuffers(std::vector<vk::Framebuffer> &fb) {
  auto &images = ds.submit_pool.get_backbuffer_views();
  auto &gbuf = frame_data.get_gbuffer();
  auto ext = ds.ctx.get_swapchain_extent();

  for (u32 i = 0; i < images.size(); i++) {
    
    auto attachments = {gbuf.images[0]->api_view(), gbuf.images[1]->api_view(), gbuf.images[2]->api_view(), images[i]};
    
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
  auto clear_vals = {color, color, depth_clear, color};

  vk::RenderPassBeginInfo info {};
  info.setRenderPass(ds.main_renderpass);
  info.setFramebuffer(dctx.backbuffer);
  info.setClearValues(clear_vals);
  info.setRenderArea(vk::Rect2D{{0, 0}, ds.ctx.get_swapchain_extent()});
      
  dctx.dcb.beginRenderPass(info, vk::SubpassContents::eInline);

  gbuffer_subpass->render(dctx, ds);
  
  dctx.dcb.nextSubpass(vk::SubpassContents::eInline);

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