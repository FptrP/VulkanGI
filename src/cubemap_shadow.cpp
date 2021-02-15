#include "cubemap_shadow.hpp"

#include "drv/cmd_utils.hpp"

void CubemapShadowRenderer::create_renderpass(DriverState &ds) {
  vk::AttachmentDescription distance_desc {}, depth_desc {};

  distance_desc
    .setFormat(vk::Format::eR32Sfloat)
    .setLoadOp(vk::AttachmentLoadOp::eClear)
    .setStoreOp(vk::AttachmentStoreOp::eStore)
    .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
    .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
    .setInitialLayout(vk::ImageLayout::eUndefined)
    .setFinalLayout(vk::ImageLayout::eTransferSrcOptimal);
  
  depth_desc = distance_desc;
  depth_desc
    .setFormat(vk::Format::eD24UnormS8Uint)
    .setStoreOp(vk::AttachmentStoreOp::eDontCare)
    .setInitialLayout(vk::ImageLayout::eUndefined)
    .setFinalLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal);

  auto attachments = {distance_desc, depth_desc};

  vk::AttachmentReference distance_ref {}, depth_ref {};
  distance_ref.setAttachment(0).setLayout(vk::ImageLayout::eColorAttachmentOptimal);
  depth_ref.setAttachment(1).setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

  auto color_attachments = {distance_ref};

  vk::SubpassDescription subpass {};
  subpass
    .setColorAttachments(color_attachments)
    .setPDepthStencilAttachment(&depth_ref)
    .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
  
  auto subpasses = {subpass};

  vk::RenderPassCreateInfo info {};
  info
    .setAttachments(attachments)
    .setSubpasses(subpasses);

  renderpass = ds.ctx.get_device().createRenderPass(info); 
}

void CubemapShadowRenderer::create_pipeline_layout(DriverState &ds) {
  drv::DescriptorSetLayoutBuilder builder {};

  builder
    .add_ubo(0, vk::ShaderStageFlagBits::eVertex)
    .add_storage_buffer(1, vk::ShaderStageFlagBits::eVertex);
  
  shader_input = ds.descriptors.create_layout(ds.ctx, builder.build(), 1);

  auto layouts = {ds.descriptors.get(shader_input)};

  vk::PushConstantRange pc {};
  pc.setOffset(0).setSize(sizeof(u32)).setStageFlags(vk::ShaderStageFlagBits::eVertex);

  auto ranges = {pc};

  vk::PipelineLayoutCreateInfo layout_info {};
  layout_info
    .setPushConstantRanges(ranges)
    .setSetLayouts(layouts);

  pipeline_layout = ds.ctx.get_device().createPipelineLayout(layout_info);

  ubo = ds.storage.create_buffer(ds.ctx, drv::GPUMemoryT::Coherent, sizeof(UBOData), vk::BufferUsageFlagBits::eUniformBuffer);
}

void CubemapShadowRenderer::create_pipeline(DriverState &ds) {
  ds.pipelines.load_shader(ds.ctx, "cube_shadow_vs", "src/shaders/cube_shadow_vert.spv", vk::ShaderStageFlagBits::eVertex);
  ds.pipelines.load_shader(ds.ctx, "cube_shadow_fs", "src/shaders/cube_shadow_frag.spv", vk::ShaderStageFlagBits::eFragment);

  drv::PipelineDescBuilder builder {};
  builder
    .add_shader("cube_shadow_vs")
    .add_shader("cube_shadow_fs")
    
    .add_attribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(SceneVertex, pos))
    .add_binding(0, sizeof(SceneVertex), vk::VertexInputRate::eVertex)

    .set_vertex_assembly(vk::PrimitiveTopology::eTriangleList, false)
    .set_polygon_mode(vk::PolygonMode::eFill)
    .set_front_face(vk::FrontFace::eCounterClockwise)

    .add_viewport(0.f, 0.f, 100, 100, 0.f, 1.f)
    .add_scissors(0, 0, 100, 100)

    .set_blend_logic_op(false)
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

void CubemapShadowRenderer::release(DriverState &ds) {
  ds.pipelines.free_pipeline(ds.ctx, pipeline);
  //ds.ctx.get_device().destroyPipelineLayout(pipeline_layout);
}

void CubemapShadowRenderer::set_shader_input(DriverState &ds, const Scene &scene) {
  shader_res = ds.descriptors.allocate_set(ds.ctx, shader_input);

  drv::DescriptorBinder bind {ds.descriptors.get(shader_res)};
  bind
    .bind_ubo(0, ubo->api_buffer())
    .bind_storage_buff(1, scene.get_matrix_buff()->api_buffer())
    .write(ds.ctx);
}

void CubemapShadowRenderer::init(DriverState &ds) {
  create_renderpass(ds);
  create_pipeline_layout(ds);
  create_pipeline(ds);
}

void CubemapShadowRenderer::render(DriverState &ds, drv::ImageID &cubemap, const Scene &scene, glm::vec3 pos) {
  set_shader_input(ds, scene);

  const auto &img_info = cubemap->get_info();
  auto ext = img_info.extent;

  auto side_img = ds.storage.create_rt(ds.ctx, ext.width, ext.height, 
    vk::Format::eR32Sfloat, vk::ImageUsageFlagBits::eColorAttachment|vk::ImageUsageFlagBits::eTransferSrc);
  
  auto depth_img = ds.storage.create_depth2D_rt(ds.ctx, ext.width, ext.height);

  auto side_view = ds.storage.create_rt_view(ds.ctx, side_img, vk::ImageAspectFlagBits::eColor);
  auto depth_view = ds.storage.create_rt_view(ds.ctx, depth_img, vk::ImageAspectFlagBits::eDepth);

  auto attachments = {side_view->api_view(), depth_view->api_view()};

  vk::FramebufferCreateInfo fbinfo {};
  fbinfo
    .setAttachments(attachments)
    .setWidth(ext.width)
    .setHeight(ext.height)
    .setRenderPass(renderpass)
    .setLayers(1);

  auto fb = ds.ctx.get_device().createFramebuffer(fbinfo);

  
  for (u32 side = 0; side < 6; side++) {
    
    UBOData data;
    data.camera_origin = glm::vec4{pos.x, pos.y, pos.z, 0.f};
    calc_matrix(side, vk::Extent2D{ext.width, ext.height}, pos, data.camera_proj);
    ds.storage.buffer_memcpy(ds.ctx, ubo, 0, &data, sizeof(data));

    auto cmd = ds.submit_pool.start_cmd(ds.ctx);
    
    vk::CommandBufferBeginInfo begin_buf {};
    cmd.begin(begin_buf);

    if (side == 0) {
      drv::ImageBarrier barrier {cubemap, vk::ImageAspectFlagBits::eColor};
      barrier
        .set_range(0, 1, 0, 6)
        .change_layout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal)
        .access_msk(vk::AccessFlagBits::eMemoryRead|vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eTransferRead)
        .write(cmd, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer);
    }

    vk::ClearValue clear_dist {}, clear_depth {};
    
    clear_depth.depthStencil.setDepth(1.f).setStencil(0u);
    clear_dist.color.setFloat32({100.f, 0.f, 0.f, 0.f});

    auto clear_vals = {clear_dist, clear_depth};

    vk::RenderPassBeginInfo pass_begin {};
    pass_begin
      .setRenderPass(renderpass)
      .setClearValues(clear_vals)
      .setFramebuffer(fb)
      .setRenderArea(vk::Rect2D{{0u, 0u}, {ext.width, ext.height}});

    cmd.beginRenderPass(pass_begin, vk::SubpassContents::eInline);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ds.pipelines.get(pipeline));
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, {ds.descriptors.get(shader_res)}, {});

    vk::Viewport viewport;
    viewport
      .setWidth(ext.width)
      .setHeight(ext.height)
      .setMinDepth(0.f)
      .setMaxDepth(1.f);

    cmd.setViewport(0, {viewport});
    cmd.setScissor(0, {vk::Rect2D{{0u, 0u}, {ext.width, ext.height}}});

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

    drv::BlitImage blit {side_img, cubemap};
    blit
      .src_subresource(vk::ImageAspectFlagBits::eColor, 0, 0, 1)
      .dst_subresource(vk::ImageAspectFlagBits::eColor, 0, side, 1)
      .src_offset(vk::Offset3D{0, 0, 0}, vk::Offset3D{(i32)ext.width, (i32)ext.height, 1})
      .dst_offset(vk::Offset3D{0, 0, 0}, vk::Offset3D{(i32)ext.width, (i32)ext.height, 1})
      .write(cmd);

    if (side == 5) {
      drv::ImageBarrier barrier {cubemap, vk::ImageAspectFlagBits::eColor};
      barrier
        .set_range(0, 1, 0, 6)
        .change_layout(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal)
        .access_msk(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead|vk::AccessFlagBits::eMemoryWrite)
        .write(cmd, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands);
    }

    cmd.end();

    auto fence = ds.submit_pool.submit_cmd(ds.ctx, cmd);
    ds.ctx.get_device().waitForFences({fence}, VK_TRUE, UINT64_MAX);
    ds.ctx.get_device().destroyFence(fence);
    ds.submit_pool.free_cmd(ds.ctx, cmd);
  }

  ds.ctx.get_device().destroyFramebuffer(fb);
}

void CubemapShadowRenderer::calc_matrix(u32 side, vk::Extent2D ext, glm::vec3 pos, glm::mat4 &out) {
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

drv::ImageViewID CubemapShadowRenderer::create_side_view(DriverState &ds, u32 side, drv::ImageID &cubemap) {


}