#include "lightfield_probes.hpp"
#include "drv/cmd_utils.hpp"

const u32 CUBEMAP_RES = 128;
const vk::Extent2D CUBEMAP_EXT {CUBEMAP_RES, CUBEMAP_RES};
const u32 OCT_RES = 256;

void LightField::calc_matrix(u32 side, vk::Extent2D ext, glm::vec3 pos, glm::mat4 &out) {
  float aspect = float(ext.width)/float(ext.height);

  glm::mat4 proj = glm::perspective(glm::radians(90.f), aspect, 0.01f, 10.f);

  glm::vec3 fwd, up;
  switch(side) {
    case 0: fwd = glm::vec3{1.f, 0.f, 0.f}; up = glm::vec3{0.f, -1.f, 0.f}; break;
    case 1: fwd = glm::vec3{-1.f, 0.f, 0.f}; up = glm::vec3{0.f, -1.f, 0.f}; break;
    case 2: fwd = glm::vec3{0.f, 1.f, 0.f}; up = glm::vec3{0.f, 0.f, 1.f}; break;
    case 3: fwd = glm::vec3{0.f, -1.f, 0.f}; up = glm::vec3{0.f, 0.f, -1.f}; break;
    case 4: fwd = glm::vec3{0.f, 0.f, 1.f}; up = glm::vec3{0.f, -1.f, 0.f}; break;
    case 5: fwd = glm::vec3{0.f, 0.f, -1.f}; up = glm::vec3{0.f, -1.f, 0.f}; break;

  }
  glm::mat4 camera = glm::lookAt(pos, pos + fwd, up);
  out = proj * camera;
}

void LightField::create_renderpass(DriverState &ds) {
  std::array<vk::AttachmentDescription, 4> desc {};

  desc[0].setInitialLayout(vk::ImageLayout::eUndefined);
  desc[0].setFinalLayout(vk::ImageLayout::eTransferSrcOptimal);
  desc[0].setLoadOp(vk::AttachmentLoadOp::eClear);
  desc[0].setStoreOp(vk::AttachmentStoreOp::eStore);
  desc[0].setSamples(vk::SampleCountFlagBits::e1);
  desc[0].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
  desc[0].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);

  desc[1] = desc[2] = desc[3] = desc[0];

  desc[0].setFormat(vk::Format::eR32Sfloat); //distance
  desc[1].setFormat(vk::Format::eR16G16B16A16Sfloat); //radiance
  desc[2].setFormat(vk::Format::eR16G16B16A16Sfloat); //normal
  desc[3].setFormat(vk::Format::eD24UnormS8Uint); //depth

  desc[3].setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

  std::array<vk::AttachmentReference, 3> inputs {}; 
  for (u32 i = 0; i < 3; i++) {
    inputs[i].setAttachment(i);
    inputs[i].setLayout(vk::ImageLayout::eColorAttachmentOptimal);
  }

  vk::AttachmentReference depth;
  depth.setAttachment(3);
  depth.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

  std::array<vk::SubpassDescription, 1> subpasses {};
  subpasses[0].setColorAttachments(inputs);
  subpasses[0].setPDepthStencilAttachment(&depth);
  subpasses[0].setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);


  vk::RenderPassCreateInfo info {};
  info.setAttachments(desc);
  info.setSubpasses(subpasses);

  renderpass = ds.ctx.get_device().createRenderPass(info);
}

void LightField::create_framebuffer(DriverState &ds) {
  const auto IMG_USG = vk::ImageUsageFlagBits::eColorAttachment|vk::ImageUsageFlagBits::eTransferSrc;
  const auto DEPTH_USG = vk::ImageUsageFlagBits::eDepthStencilAttachment;
  const auto CM_USAGE = vk::ImageUsageFlagBits::eTransferDst|vk::ImageUsageFlagBits::eSampled;

  auto dist_img = ds.storage.create_rt(ds.ctx, CUBEMAP_RES, CUBEMAP_RES, vk::Format::eR32Sfloat, IMG_USG);
  auto color_img = ds.storage.create_rt(ds.ctx, CUBEMAP_RES, CUBEMAP_RES, vk::Format::eR16G16B16A16Sfloat, IMG_USG);
  auto norm_img = ds.storage.create_rt(ds.ctx, CUBEMAP_RES, CUBEMAP_RES, vk::Format::eR16G16B16A16Sfloat, IMG_USG);
  auto depth_img = ds.storage.create_rt(ds.ctx, CUBEMAP_RES, CUBEMAP_RES, vk::Format::eD24UnormS8Uint, DEPTH_USG);

  dist = ds.storage.create_rt_view(ds.ctx, dist_img, vk::ImageAspectFlagBits::eColor);
  norm = ds.storage.create_rt_view(ds.ctx, norm_img, vk::ImageAspectFlagBits::eColor);
  color = ds.storage.create_rt_view(ds.ctx, color_img, vk::ImageAspectFlagBits::eColor);
  depth = ds.storage.create_rt_view(ds.ctx, depth_img, vk::ImageAspectFlagBits::eDepth);

  auto dist_cm = ds.storage.create_cubemap(ds.ctx, CUBEMAP_RES, CUBEMAP_RES, vk::Format::eR32Sfloat, CM_USAGE);
  auto color_cm = ds.storage.create_cubemap(ds.ctx, CUBEMAP_RES, CUBEMAP_RES, vk::Format::eR16G16B16A16Sfloat, CM_USAGE);
  auto norm_cm = ds.storage.create_cubemap(ds.ctx, CUBEMAP_RES, CUBEMAP_RES, vk::Format::eR16G16B16A16Sfloat, CM_USAGE);

  cm_dist = ds.storage.create_cubemap_view(ds.ctx, dist_cm, vk::ImageAspectFlagBits::eColor);
  cm_norm = ds.storage.create_cubemap_view(ds.ctx, norm_cm, vk::ImageAspectFlagBits::eColor);
  cm_color = ds.storage.create_cubemap_view(ds.ctx, color_cm, vk::ImageAspectFlagBits::eColor);

  std::array<vk::ImageView, 4> fb_views {dist->api_view(), color->api_view(), norm->api_view(), depth->api_view()};

  vk::FramebufferCreateInfo fb_info {};
  fb_info.setAttachments(fb_views);
  fb_info.setWidth(CUBEMAP_RES);
  fb_info.setHeight(CUBEMAP_RES);
  fb_info.setLayers(1);
  fb_info.setRenderPass(renderpass);
  
  fb = ds.ctx.get_device().createFramebuffer(fb_info);

  vk::SamplerCreateInfo smp {};
  smp
    .setMinFilter(vk::Filter::eLinear)
    .setMagFilter(vk::Filter::eLinear)
    .setMipmapMode(vk::SamplerMipmapMode::eLinear)
    .setMinLod(0.f)
    .setMaxLod(10.f);
    
  sampler = ds.ctx.get_device().createSampler(smp);
}


void LightField::create_pipeline_layout(DriverState &ds) {
  drv::DescriptorSetLayoutBuilder builder {};
  builder
    .add_ubo(0, vk::ShaderStageFlagBits::eVertex)
    .add_storage_buffer(1, vk::ShaderStageFlagBits::eVertex);
  
  resource_desc = ds.descriptors.create_layout(ds.ctx, builder.build(), 1);
  resource_set = ds.descriptors.allocate_set(ds.ctx, resource_desc);

  vk::PushConstantRange range {};
  range.setOffset(0);
  range.setSize(sizeof(u32));
  range.setStageFlags(vk::ShaderStageFlagBits::eVertex);

  auto set_layouts = { ds.descriptors.get(resource_desc) };
  auto ranges = { range };

  vk::PipelineLayoutCreateInfo info {};
  info
    .setSetLayouts(set_layouts)
    .setPushConstantRanges(ranges);
  
  pipeline_layout = ds.ctx.get_device().createPipelineLayout(info);

  ubo = ds.storage.create_buffer(ds.ctx, drv::GPUMemoryT::Coherent, sizeof(UBOData), vk::BufferUsageFlagBits::eUniformBuffer);
}

void LightField::create_pipeline(DriverState &ds) {
  ds.pipelines.load_shader(ds.ctx, "cube_probe_vs", "src/shaders/cube_probe_vert.spv", vk::ShaderStageFlagBits::eVertex);
  ds.pipelines.load_shader(ds.ctx, "cube_probe_fs", "src/shaders/cube_probe_frag.spv", vk::ShaderStageFlagBits::eFragment);

  drv::PipelineDescBuilder builder {};
  builder
    .add_shader("cube_probe_vs")
    .add_shader("cube_probe_fs")
    
    .add_attribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(SceneVertex, pos))
    .add_binding(0, sizeof(SceneVertex), vk::VertexInputRate::eVertex)

    .set_vertex_assembly(vk::PrimitiveTopology::eTriangleList, false)
    .set_polygon_mode(vk::PolygonMode::eFill)
    .set_front_face(vk::FrontFace::eCounterClockwise)

    .add_viewport(0.f, 0.f, 100, 100, 0.f, 1.f)
    .add_scissors(0, 0, 100, 100)

    .set_blend_logic_op(false)
    .add_blend_attachment()
    .add_blend_attachment()
    .add_blend_attachment()
    .set_blend_constants(1.f, 1.f, 1.f, 1.f)

    .set_depth_test(true)
    .set_depth_func(vk::CompareOp::eLessOrEqual)
    .set_depth_write(true)

    .add_dynamic_state(vk::DynamicState::eViewport)
    .add_dynamic_state(vk::DynamicState::eScissor)

    .attach_to_renderpass(renderpass, 0)
    .set_layout(pipeline_layout);
  
  pipeline = ds.pipelines.create_pipeline(ds.ctx, builder);
}

void LightField::init(DriverState &ds) {
  create_renderpass(ds);
  create_framebuffer(ds);
  create_pipeline_layout(ds);
  create_pipeline(ds);

  ds.pipelines.load_shader(ds.ctx, "pass_vs", "src/shaders/pass_vert.spv", vk::ShaderStageFlagBits::eVertex);
  ds.pipelines.load_shader(ds.ctx, "cube_probe_to_oct_fs", "src/shaders/cube_probe_to_oct_frag.spv", vk::ShaderStageFlagBits::eFragment);

  lightprobe_pass
    .init_attachment(vk::Format::eR32Sfloat)
    .init_attachment(vk::Format::eR16G16B16A16Sfloat)
    .init_attachment(vk::Format::eR16G16B16A16Sfloat)
    .init(ds, 3, "cube_probe_to_oct_fs");
  
}

void LightField::release(DriverState &ds) {
  lightprobe_pass.release(ds);
  ds.ctx.get_device().destroySampler(sampler);
  ds.ctx.get_device().destroyPipelineLayout(pipeline_layout);
  ds.ctx.get_device().destroyFramebuffer(fb);
  ds.ctx.get_device().destroyRenderPass(renderpass);
}

void LightField::render_cubemaps(DriverState &ds, Scene &scene, glm::vec3 center) {
  for (u32 side = 0; side < 6; side++) {
    UBOData data;
    data.camera_origin = glm::vec4{center.x, center.y, center.z, 0.f};
    calc_matrix(side, vk::Extent2D{CUBEMAP_RES, CUBEMAP_RES}, center, data.camera_proj);
    ds.storage.buffer_memcpy(ds.ctx, ubo, 0, &data, sizeof(data));

    auto cmd = ds.submit_pool.start_cmd(ds.ctx);
    
    vk::CommandBufferBeginInfo begin_buf {};
    cmd.begin(begin_buf);

    if (side == 0) {
      transform_cubemap_layout(cmd, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    }

    vk::ClearValue clear_dist {}, clear_depth {}, clear_color {};
    
    clear_depth.depthStencil.setDepth(1.f).setStencil(0u);
    clear_dist.color.setFloat32({100.f, 0.f, 0.f, 0.f});
    clear_color.color.setFloat32({0.f, 0.f, 0.f, 0.f});
    auto clear_vals = {clear_dist, clear_color, clear_color, clear_depth};

    vk::RenderPassBeginInfo pass_begin {};
    pass_begin
      .setRenderPass(renderpass)
      .setClearValues(clear_vals)
      .setFramebuffer(fb)
      .setRenderArea(vk::Rect2D{{0u, 0u}, CUBEMAP_EXT});

    cmd.beginRenderPass(pass_begin, vk::SubpassContents::eInline);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ds.pipelines.get(pipeline));
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, {ds.descriptors.get(resource_set)}, {});

    vk::Viewport viewport;
    viewport
      .setWidth(CUBEMAP_RES)
      .setHeight(CUBEMAP_RES)
      .setMinDepth(0.f)
      .setMaxDepth(1.f);

    cmd.setViewport(0, {viewport});
    cmd.setScissor(0, {vk::Rect2D{{0u, 0u}, CUBEMAP_EXT}});

    auto buffers = { scene.get_verts_buff()->api_buffer() };
    auto offsets = {0ul};
    
    cmd.bindVertexBuffers(0, buffers, offsets);
    cmd.bindIndexBuffer(scene.get_index_buff()->api_buffer(), 0, vk::IndexType::eUint32);

    const auto& objects = scene.get_objects(); 
    auto &materials = scene.get_material_desc();
    for (auto &obj : objects) {
      if (materials[obj.material_index].albedo_path.empty()) continue;
      u32 mat_id = obj.matrix_index;
      cmd.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0u, sizeof(u32), &mat_id);
      cmd.drawIndexed(obj.index_count, 1, obj.index_offset, obj.vertex_offset, 0);
    }    


    cmd.endRenderPass();

    blit_cubemaps(cmd, side);
    if (side == 5) {
      transform_cubemap_layout(cmd, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
    }

    cmd.end();

    auto fence = ds.submit_pool.submit_cmd(ds.ctx, cmd);
    ds.ctx.get_device().waitForFences({fence}, VK_TRUE, UINT64_MAX);
    ds.ctx.get_device().destroyFence(fence);
    ds.submit_pool.free_cmd(ds.ctx, cmd);
  }
}

void LightField::bind_resources(DriverState &ds, Scene &scene) {
  drv::DescriptorBinder bind {ds.descriptors.get(resource_set)};
  bind
    .bind_ubo(0, ubo->api_buffer())
    .bind_storage_buff(1, scene.get_matrix_buff()->api_buffer());
  
  bind.write(ds.ctx);
}

void LightField::render(DriverState &ds, Scene &scene, glm::vec3 center, glm::vec3 step, glm::uvec3 d) {
  dim = d;
  
  bind_resources(ds, scene);
  probes.reserve(d.x * d.y * d.z);

  glm::vec3 size {d.x * step.x, d.y * step.y, d.z * step.z};
  glm::vec3 origin = center - 0.5f * size;

  for (u32 x = 0; x < d.x; x++) {
    for (u32 y = 0; y < d.y; y++) {
      for (u32 z = 0; z < d.z; z++) {
        glm::vec3 pos = origin + glm::vec3{step.x * x, step.y * y, step.z * z};
        render_cubemaps(ds, scene, pos);
        
        LightFieldProbe probe;
        probe.pos = pos;
        const auto USG = vk::ImageUsageFlagBits::eColorAttachment|vk::ImageUsageFlagBits::eSampled;
        auto pdist = ds.storage.create_rt(ds.ctx, OCT_RES, OCT_RES, vk::Format::eR32Sfloat, USG);
        auto pcolor = ds.storage.create_rt(ds.ctx, OCT_RES, OCT_RES, vk::Format::eR16G16B16A16Sfloat, USG);
        auto pnrom = ds.storage.create_rt(ds.ctx, OCT_RES, OCT_RES, vk::Format::eR16G16B16A16Sfloat, USG);

        probe.color = ds.storage.create_rt_view(ds.ctx, pcolor, vk::ImageAspectFlagBits::eColor);
        probe.dist = ds.storage.create_rt_view(ds.ctx, pdist, vk::ImageAspectFlagBits::eColor);
        probe.norm = ds.storage.create_rt_view(ds.ctx, pnrom, vk::ImageAspectFlagBits::eColor);

        lightprobe_pass.set_render_area(OCT_RES, OCT_RES);
        lightprobe_pass.set_attachment(0, probe.dist);
        lightprobe_pass.set_attachment(1, probe.color);
        lightprobe_pass.set_attachment(2, probe.norm);
        lightprobe_pass.set_image_sampler(0, cm_dist, sampler);
        lightprobe_pass.set_image_sampler(1, cm_color, sampler);
        lightprobe_pass.set_image_sampler(2, cm_norm, sampler);
        lightprobe_pass.render_and_wait(ds);
        probes.push_back(probe);
      }
    }
  }
}

void LightField::transform_cubemap_layout(vk::CommandBuffer &buf, vk::ImageLayout src, vk::ImageLayout dst) {
  std::array<drv::ImageID, 3> cubemaps {
    cm_dist->get_base_img(),
    cm_color->get_base_img(),
    cm_norm->get_base_img()};
  
  vk::PipelineStageFlags pipe_src, pipe_dst;
  vk::AccessFlags a_src, a_dst;

  if (src == vk::ImageLayout::eUndefined && dst == vk::ImageLayout::eTransferDstOptimal) {
    pipe_src = vk::PipelineStageFlagBits::eAllCommands;
    pipe_dst = vk::PipelineStageFlagBits::eTransfer;
    a_src = vk::AccessFlagBits::eMemoryRead|vk::AccessFlagBits::eMemoryWrite;
    a_dst = vk::AccessFlagBits::eTransferRead;
  }

  if (src == vk::ImageLayout::eTransferDstOptimal && dst == vk::ImageLayout::eShaderReadOnlyOptimal) {
    pipe_src = vk::PipelineStageFlagBits::eTransfer;
    pipe_dst = vk::PipelineStageFlagBits::eAllCommands;
    a_src = vk::AccessFlagBits::eTransferWrite;
    a_dst = vk::AccessFlagBits::eMemoryRead|vk::AccessFlagBits::eMemoryWrite;
  }

  for (u32 i = 0; i < cubemaps.size(); i++) {
    drv::ImageBarrier barrier {cubemaps[i], vk::ImageAspectFlagBits::eColor};
    barrier
      .set_range(0, 1, 0, 6)
      .change_layout(src, dst)
      .access_msk(a_src, a_dst)
      .write(buf, pipe_src, pipe_dst);
  }
}

void LightField::blit_cubemaps(vk::CommandBuffer &buf, u32 side) {
  std::array<drv::ImageID, 3> cubemaps {
    cm_dist->get_base_img(),
    cm_color->get_base_img(),
    cm_norm->get_base_img()};
  
  std::array<drv::ImageID, 3> images {
    dist->get_base_img(),
    color->get_base_img(),
    norm->get_base_img()};

  for (u32 i = 0; i < cubemaps.size(); i++) {
    drv::BlitImage blit {images[i], cubemaps[i]};
    blit
      .src_subresource(vk::ImageAspectFlagBits::eColor, 0, 0, 1)
      .dst_subresource(vk::ImageAspectFlagBits::eColor, 0, side, 1)
      .src_offset(vk::Offset3D{0, 0, 0}, vk::Offset3D{(i32)CUBEMAP_RES, (i32)CUBEMAP_RES, 1})
      .dst_offset(vk::Offset3D{0, 0, 0}, vk::Offset3D{(i32)CUBEMAP_RES, (i32)CUBEMAP_RES, 1})
      .write(buf);
  }
}