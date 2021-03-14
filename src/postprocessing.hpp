#ifndef POSTPROCESSING_HPP_INCLUDED
#define POSTPROCESSING_HPP_INCLUDED


#include "driverstate.hpp"

struct Nil {};

template <typename Arg>
struct isEmpty;

template <>
struct isEmpty<Nil> {
  static constexpr bool val = true;
};

template <typename Arg>
struct isEmpty {
  static constexpr bool val = std::is_same<Nil, Arg>::val;
};

template <typename FrameData = Nil, typename PushData = Nil>
struct PostProcessingPass {

  PostProcessingPass &init_attachment(vk::Format fmt) {
    attachment_fmts.push_back(fmt);
    return *this;
  }

  void init(DriverState &ds, u32 input_textures_count, const std::string &fs_name) {
    if (attachment_fmts.size() <= 0) {
      throw std::runtime_error {"Invalid renderpass with no attachments"};
    }

    image_bindings.resize(input_textures_count);
    create_renderpass(ds);
    create_pipeline_layout(ds);
    create_pipeline(ds, "pass_vs", fs_name);

    mark_framebuffers_dirty();
    mark_sets_dirty();
  }

  void release(DriverState &ds) {
    image_bindings.clear();

    ds.descriptors.free_layout(ds.ctx, res_layout);
    for (u32 i = 0; i < CONTEXTS_COUNT; i++) {
      if (framebuffers[i]) {
        ds.ctx.get_device().destroyFramebuffer(framebuffers[i]);
        framebuffers[i] = nullptr;
      }

      ubo[i].release();
    }
    ds.pipelines.free_pipeline(ds.ctx, pipeline);
    ds.ctx.get_device().destroyRenderPass(renderpass);
  }

  void set_frame_data(const FrameData &data) {
    if constexpr(!supportsUBO()) return;
    next_frame_data = data;
    mark_sets_dirty();
  }

  void set_push_const(const PushData &data) {
    if constexpr(!supportsPC()) return;
    next_push_data = data;
    mark_sets_dirty();
  }

  void set_image_sampler(u32 binding, drv::ImageViewID id, vk::Sampler sampler) { 
    image_bindings[binding].smp = sampler;
    image_bindings[binding].img = id;
    mark_sets_dirty();
  }

  void set_render_area(u32 w, u32 h) {
    width = w;
    height = h;
  }

  void set_attachment(u32 binding, drv::ImageViewID id) {
    attachments[binding] = id;
    mark_framebuffers_dirty();
  }

  void render(DriverState &ds, drv::DrawContext &dctx) {
    flush(ds, dctx.frame_id);
    bind_and_draw(ds, dctx.frame_id, dctx.dcb);
  }

  void render_and_wait(DriverState &ds) {
    flush(ds, SEQ_CTX);
    auto cmd = ds.submit_pool.start_cmd(ds.ctx);

    vk::CommandBufferBeginInfo begin_info {};
    cmd.begin(begin_info);
    bind_and_draw(ds, SEQ_CTX, cmd);
    cmd.end();
    auto fence = ds.submit_pool.submit_cmd(ds.ctx, cmd);
    ds.ctx.get_device().waitForFences({fence}, VK_TRUE, ~(0ul));
    ds.ctx.get_device().destroyFence(fence);
  }

  

private:
  static constexpr u32 CONTEXTS_COUNT = drv::MAX_FRAMES_IN_FLIGHT + 1;   
  static constexpr u32 SEQ_CTX = CONTEXTS_COUNT - 1; 

  static inline constexpr bool supportsUBO() {
    return !isEmpty<FrameData>::val;
  }

  static inline constexpr bool supportsPC() {
    return !isEmpty<PushData>::val;
  }

  void mark_sets_dirty() {
    for (u32 i = 0; i < CONTEXTS_COUNT; i++)
      set_dirty[i] = true;
  }

  void mark_framebuffers_dirty() {
    for (u32 i = 0; i < CONTEXTS_COUNT; i++)
      framebuffer_dirty[i] = true;
  }

  void flush(DriverState &ds, u32 ctx_id) {
    if (framebuffer_dirty[ctx_id]) {
      if (framebuffers[ctx_id]) {
        ds.ctx.get_device().destroyFramebuffer(framebuffers[ctx_id]);
        framebuffers[ctx_id] = nullptr;
      }

      std::vector<vk::ImageView> api_views;
      api_views.resize(attachments.size());
      for (u32 i = 0; i < attachments.size(); i++) {
        if (attachments[i].is_nullptr()) {
          throw std::runtime_error {"Missed attachment in framebuffer!"};
        }
        api_views[i] = attachments[i]->api_view();
      }

      vk::FramebufferCreateInfo info {};
      info.setAttachments(api_views);
      info.setLayers(1);
      info.setRenderPass(renderpass);
      info.setWidth(width);
      info.setHeight(height);

      framebuffers[ctx_id] = ds.ctx.get_device().createFramebuffer(info);
      framebuffer_dirty[ctx_id] = false;
    }

    if (set_dirty[ctx_id]) {
      const u32 img_slots = image_bindings.size();
      auto set = ds.descriptors.get(sets[ctx_id]);
      drv::DescriptorBinder binder {set};
      
      for (u32 i = 0; i < img_slots; i++) {
        if (image_bindings[i].img.is_nullptr()) {
          throw std::runtime_error {"Empty texture is binded to shader!"};
        }
        binder.bind_combined_img(i, image_bindings[i].img->api_view(), image_bindings[i].smp);
      }

      if constexpr (supportsUBO()) {
        binder.bind_ubo(img_slots, ubo[ctx_id]->api_buffer());
      }
      binder.write(ds.ctx);

      set_dirty[ctx_id] = false;
    }
  }

  void create_renderpass(DriverState &ds) {
    vk::AttachmentDescription base_desc {};
    base_desc
      .setInitialLayout(vk::ImageLayout::eUndefined)
      .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
      .setLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStoreOp(vk::AttachmentStoreOp::eStore)
      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setSamples(vk::SampleCountFlagBits::e1);

    u32 attach_count = attachment_fmts.size();

    std::vector<vk::AttachmentDescription> attach_desc {};
    std::vector<vk::AttachmentReference> attach_ref {};

    attach_desc.resize(attach_count);
    attach_ref.resize(attach_count);
    for (u32 i = 0; i < attach_count; i++) {
      attach_desc[i] = base_desc;
      attach_desc[i].setFormat(attachment_fmts[i]);

      attach_ref[i].setLayout(vk::ImageLayout::eColorAttachmentOptimal);
      attach_ref[i].setAttachment(i);
    }

    attachments.resize(attachment_fmts.size());

    vk::SubpassDescription subpass {};
    subpass
      .setColorAttachments(attach_ref)
      .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

    auto subpasses = {subpass};

    vk::RenderPassCreateInfo info {};
    info.setAttachments(attach_desc);
    info.setSubpasses(subpasses);

    renderpass = ds.ctx.get_device().createRenderPass(info);
  }

  void create_pipeline_layout(DriverState &ds) {
    if constexpr(supportsUBO()) {
      for (u32 i = 0; i < CONTEXTS_COUNT; i++) {
        ubo[i] = ds.storage.create_buffer(ds.ctx, drv::GPUMemoryT::Coherent, sizeof(FrameData), vk::BufferUsageFlagBits::eUniformBuffer);
      }
    }


    drv::DescriptorSetLayoutBuilder builder {};

    const u32 tex_count = image_bindings.size();

    for (u32 i = 0; i < tex_count; i++) {
      builder.add_combined_sampler(i, vk::ShaderStageFlagBits::eFragment);
    }

    if constexpr(supportsUBO()) {
      builder.add_ubo(tex_count, vk::ShaderStageFlagBits::eFragment);
    }

    res_layout = ds.descriptors.create_layout(ds.ctx, builder.build(), CONTEXTS_COUNT);
    
    for (u32 i = 0; i < CONTEXTS_COUNT; i++) {
      sets[i] = ds.descriptors.allocate_set(ds.ctx, res_layout);
    }

    auto desc_set_layouts = {ds.descriptors.get(res_layout)};

    vk::PipelineLayoutCreateInfo info {};
    vk::PushConstantRange range {};
    if constexpr(supportsPC()) {
      range.setOffset(0);
      range.setSize(sizeof(PushData));
      range.setStageFlags(vk::ShaderStageFlagBits::eFragment);
    }

    auto ranges = {range};
    if constexpr(supportsPC()) {
      info.setPushConstantRanges(ranges);
    }
    
    info.setSetLayouts(desc_set_layouts);
    pipeline_layout = ds.ctx.get_device().createPipelineLayout(info);
  }

  void create_pipeline(DriverState &ds, const std::string &vert_name, const std::string &frag_name) {
    drv::PipelineDescBuilder builder {};
    builder
      .add_shader(vert_name)
      .add_shader(frag_name)
    
      .set_vertex_assembly(vk::PrimitiveTopology::eTriangleList, false)
      .set_polygon_mode(vk::PolygonMode::eFill)
      .set_front_face(vk::FrontFace::eCounterClockwise)

      .add_viewport(0.f, 0.f, 1.f, 1.f, 0.f, 1.f)
      .add_scissors(0, 0, 1, 1)

      .set_blend_logic_op(false)
      .set_blend_constants(1.f, 1.f, 1.f, 1.f)

      .set_depth_test(false)
      .set_depth_func(vk::CompareOp::eNever)
      .set_depth_write(false)

      .add_dynamic_state(vk::DynamicState::eViewport)
      .add_dynamic_state(vk::DynamicState::eScissor)

      .attach_to_renderpass(renderpass, 0)
      .set_layout(pipeline_layout);
    
    for (u32 i = 0; i < attachments.size(); i++) {
      builder.add_blend_attachment();
    }

    pipeline = ds.pipelines.create_pipeline(ds.ctx, builder);
  }

  void bind_and_draw(DriverState &ds, u32 id, vk::CommandBuffer &cmd) {
    vk::ClearColorValue clear_val {};
    clear_val.setFloat32({0.f, 0.f, 0.f, 0.f});
    
    std::vector<vk::ClearValue> clear;
    clear.resize(attachments.size());
    for (u32 i = 0; i < clear.size(); i++) {
      clear[i].color = clear_val;
    }

    vk::Rect2D area;
    area.offset.x = 0;
    area.offset.y = 0;
    area.extent = vk::Extent2D{width, height};

    vk::RenderPassBeginInfo rbegin {};
    rbegin
    .setRenderPass(renderpass)
    .setFramebuffer(framebuffers[id])
    .setRenderArea(area)
    .setClearValues(clear);
  
    cmd.beginRenderPass(rbegin, vk::SubpassContents::eInline);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ds.pipelines.get(pipeline));
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, {ds.descriptors.get(sets[id])}, {});
    
    vk::Viewport vp;
    vp.width = width;
    vp.height = height;
    vp.minDepth = 0.f;
    vp.maxDepth = 1.f;
    
    cmd.setViewport(0, {vp});

    vk::Rect2D sci;
    sci.extent.width = width;
    sci.extent.height = height;
    sci.offset.x = 0;
    sci.offset.y = 0;

    cmd.setScissor(0, {sci});

    if constexpr(supportsPC()) {
      cmd.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushData), &next_push_data);
    }

    cmd.draw(3, 1, 0, 0);

    cmd.endRenderPass();
  }

  drv::PipelineID pipeline;
  drv::DescriptorSetLayoutID res_layout;
  vk::PipelineLayout pipeline_layout;

  drv::DescriptorSetID sets[CONTEXTS_COUNT];
  drv::BufferID ubo[CONTEXTS_COUNT] {};
  bool set_dirty[CONTEXTS_COUNT] {true};
  
  vk::RenderPass renderpass;

  vk::Framebuffer framebuffers[CONTEXTS_COUNT];
  u32 width = 0, height = 0;
  bool framebuffer_dirty[CONTEXTS_COUNT] {true};

  PushData next_push_data {};
  FrameData next_frame_data {};

  struct ImageBinding {
    vk::Sampler smp;
    drv::ImageViewID img;
  };

  std::vector<vk::Format> attachment_fmts; 
  std::vector<ImageBinding> image_bindings;
  std::vector<drv::ImageViewID> attachments;
};

#endif