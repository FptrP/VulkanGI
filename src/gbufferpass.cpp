#include "triangle.hpp"

void GBufferSubpass::create_texture_sets(DriverState &ds) {
  vk::ImageSubresourceRange i_range {};
  i_range
    .setAspectMask(vk::ImageAspectFlagBits::eColor)
    .setBaseArrayLayer(0)
    .setBaseMipLevel(0)
    .setLayerCount(1)
    .setLevelCount(~0u);
    
  for (auto &mat : scene.get_materials()) {
    if (mat.albedo_path.empty()) continue;
    auto img = ds.storage.load_image2D(ds.ctx, mat.albedo_path.c_str());
    auto view = ds.storage.create_image_view(ds.ctx, img, vk::ImageViewType::e2D, i_range);
    images.push_back(view);
  }

  drv::DescriptorSetLayoutBuilder builder {};
  builder
    .add_sampler(0, vk::ShaderStageFlagBits::eFragment)
    .add_array_of_tex(1, images.size(), vk::ShaderStageFlagBits::eFragment);
    
  tex_layout = ds.descriptors.create_layout(ds.ctx, builder.build(), 1);
  texture_set = ds.descriptors.allocate_set(ds.ctx, tex_layout);
}

void GBufferSubpass::create_renderpass(DriverState &ds) {
  vk::AttachmentDescription albedo_desc {};
  albedo_desc
    .setInitialLayout(vk::ImageLayout::eUndefined)
    .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
    .setFormat(vk::Format::eR8G8B8A8Srgb)
    .setLoadOp(vk::AttachmentLoadOp::eClear)
    .setStoreOp(vk::AttachmentStoreOp::eStore)
    .setSamples(vk::SampleCountFlagBits::e1)
    .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
    .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);

  vk::AttachmentDescription normal_desc = albedo_desc;
  normal_desc.setFormat(vk::Format::eR16G16B16A16Sfloat);
  
  vk::AttachmentDescription worldpos_desc = normal_desc;
  
  vk::AttachmentDescription depth_desc = albedo_desc;
  depth_desc
    .setFinalLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal)
    .setFormat(vk::Format::eD24UnormS8Uint);

  auto attachments = {albedo_desc, normal_desc, worldpos_desc, depth_desc};
  vk::AttachmentReference albedo, normal, worldpos, depth;

  albedo.attachment = 0;
  albedo.layout = vk::ImageLayout::eColorAttachmentOptimal;

  normal = albedo;
  normal.attachment = 1;

  worldpos = albedo;
  worldpos.attachment = 2;

  depth.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
  depth.attachment = 3;

  auto color_att = {albedo, normal, worldpos};
  vk::SubpassDescription subpass {};
  subpass
    .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
    .setColorAttachments(color_att)
    .setPDepthStencilAttachment(&depth);

  auto subpasses = {subpass};

  vk::SubpassDependency begin {};
  begin
    .setSrcAccessMask(vk::AccessFlagBits::eMemoryRead|vk::AccessFlagBits::eMemoryWrite)
    .setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentRead)
    .setSrcStageMask(vk::PipelineStageFlagBits::eAllCommands)
    .setDstStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests)
    .setSrcSubpass(VK_SUBPASS_EXTERNAL)
    .setDstSubpass(0);
  
  vk::SubpassDependency end {};
  end  
    .setSrcAccessMask(vk::AccessFlagBits::eMemoryRead|vk::AccessFlagBits::eMemoryWrite)
    .setDstAccessMask(vk::AccessFlagBits::eMemoryRead|vk::AccessFlagBits::eMemoryWrite)
    .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
    .setDstStageMask(vk::PipelineStageFlagBits::eAllCommands)
    .setSrcSubpass(0)
    .setDstSubpass(VK_SUBPASS_EXTERNAL);

  auto dependencies = {begin, end};

  vk::RenderPassCreateInfo info {};
  info
    .setAttachments(attachments)
    .setDependencies(dependencies)
    .setSubpasses(subpasses);

  gbuf_renderpass = ds.ctx.get_device().createRenderPass(info);
}

void GBufferSubpass::create_framebuffer(DriverState &ds) {
  auto &gbuff = frame_data.get_gbuffer();
  auto ext = ds.ctx.get_swapchain_extent();

  auto attachments = {
    gbuff.images[0]->api_view(),
    gbuff.images[1]->api_view(),
    gbuff.images[2]->api_view(),
    gbuff.images[3]->api_view()
  };

  vk::FramebufferCreateInfo info {};
  info
    .setWidth(ext.width)
    .setHeight(ext.height)
    .setAttachments(attachments)
    .setLayers(1)
    .setRenderPass(gbuf_renderpass);

  framebuf = ds.ctx.get_device().createFramebuffer(info);
}

void GBufferSubpass::create_pipeline(DriverState &ds) {
  ds.pipelines.load_shader(ds.ctx, "triangle_vs", "src/shaders/triangle_vert.spv", vk::ShaderStageFlagBits::eVertex);
  ds.pipelines.load_shader(ds.ctx, "triangle_fs", "src/shaders/triangle_frag.spv", vk::ShaderStageFlagBits::eFragment);

  drv::PipelineDescBuilder desc;
  auto ext = ds.ctx.get_swapchain_extent();
  desc
    .add_shader("triangle_vs")
    .add_shader("triangle_fs")
    
    .add_attribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(SceneVertex, pos))
    .add_attribute(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(SceneVertex, norm))
    .add_attribute(2, 0, vk::Format::eR32G32Sfloat, offsetof(SceneVertex, uv))
    .add_binding(0, sizeof(SceneVertex), vk::VertexInputRate::eVertex)

    .set_vertex_assembly(vk::PrimitiveTopology::eTriangleList, false)
    .set_polygon_mode(vk::PolygonMode::eFill)
    .set_front_face(vk::FrontFace::eCounterClockwise)

    .add_viewport(0.f, 0.f, ext.width, ext.height, 0.f, 1.f)
    .add_scissors(0, 0, ext.width, ext.height)

    .set_blend_logic_op(false)
    .add_blend_attachment()
    .add_blend_attachment()
    .add_blend_attachment()
    .set_blend_constants(1.f, 1.f, 1.f, 1.f)

    .set_depth_test(true)
    .set_depth_func(vk::CompareOp::eLessOrEqual)
    .set_depth_write(true)

    .attach_to_renderpass(gbuf_renderpass, 0)
    .set_layout(pipeline_layout);
  
  pipeline = ds.pipelines.create_pipeline(ds.ctx, desc);
}

void GBufferSubpass::create_pipeline_layout(DriverState &ds) {
  create_texture_sets(ds);

  drv::DescriptorSetLayoutBuilder builder {};
  builder.add_ubo(0, vk::ShaderStageFlagBits::eVertex);
  builder.add_storage_buffer(1, vk::ShaderStageFlagBits::eVertex);
  
  desc_layout = ds.descriptors.create_layout(ds.ctx, builder.build(), drv::MAX_FRAMES_IN_FLIGHT);
  sets.push_back(ds.descriptors.allocate_set(ds.ctx, desc_layout));
  sets.push_back(ds.descriptors.allocate_set(ds.ctx, desc_layout));

  auto layouts = {ds.descriptors.get(desc_layout), ds.descriptors.get(tex_layout)};

  vk::PushConstantRange pconst {};
  pconst
    .setStageFlags(vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment)
    .setOffset(0)
    .setSize(2 * sizeof(u32));


  auto pconstants = {pconst};

  vk::PipelineLayoutCreateInfo info {};
  info.setSetLayouts(layouts);
  info.setPushConstantRanges(pconstants);

  pipeline_layout = ds.ctx.get_device().createPipelineLayout(info);

  vk::SamplerCreateInfo samp_i {};
  samp_i.setMinFilter(vk::Filter::eLinear);
  samp_i.setMagFilter(vk::Filter::eLinear);
  samp_i.setMipmapMode(vk::SamplerMipmapMode::eLinear);
  samp_i.setMinLod(0.f);
  samp_i.setMaxLod(10.f);
  sampler = ds.ctx.get_device().createSampler(samp_i);

      
  vk::ImageSubresourceRange i_range {};
  i_range
    .setAspectMask(vk::ImageAspectFlagBits::eColor)
    .setBaseArrayLayer(0)
    .setBaseMipLevel(0)
    .setLayerCount(1)
    .setLevelCount(1);


  for (u32 i = 0; i < drv::MAX_FRAMES_IN_FLIGHT; i++) {
    ubo[i] = ds.storage.create_buffer(ds.ctx, drv::GPUMemoryT::Coherent, sizeof(VertexUB), vk::BufferUsageFlagBits::eUniformBuffer);
    drv::DescriptorBinder bind {ds.descriptors.get(sets[i])};
    bind
      .bind_ubo(0, *ubo[i])
      .bind_storage_buff(1, scene.get_matrix_buff()->api_buffer());    
    bind.write(ds.ctx);
  }

  std::vector<vk::ImageView> api_views;
  for (auto &id : images) {
    api_views.push_back(id->api_view());
  }

  drv::DescriptorBinder img_bind {ds.descriptors.get(texture_set)};
  img_bind
    .bind_sampler(0, sampler)
    .bind_array_of_img(1, images.size(), api_views.data());
      
  img_bind.write(ds.ctx);      
}

void GBufferSubpass::render(drv::DrawContext &draw_ctx, DriverState &ds) {
  auto frame = draw_ctx.frame_id;
    
  VertexUB data;
  data.camera = frame_data.get_camera_matrix();
  auto camera_pos = frame_data.get_camera_pos();
  data.project = frame_data.get_projection_matrix();
  data.camera_origin = glm::vec4{camera_pos.x, camera_pos.y, camera_pos.z, 0.f};

  auto ext = ds.ctx.get_swapchain_extent();
  vk::Rect2D area {};
  area.offset = vk::Offset2D{0, 0};
  area.extent = ext;

  vk::ClearColorValue val;
  val.setFloat32({0.f, 0.f, 0.f, 0.f});

  vk::ClearDepthStencilValue depth_s;
  depth_s.setDepth(1.f).setStencil(0);

  std::array<vk::ClearValue, 4> clear_val;
  clear_val[0].setColor(val);
  clear_val[1].setColor(val);
  clear_val[2].setColor(val);
  clear_val[3].setDepthStencil(depth_s);

  vk::RenderPassBeginInfo begin_rp {};
  begin_rp
    .setClearValues(clear_val)
    .setFramebuffer(framebuf)
    .setRenderPass(gbuf_renderpass)
    .setRenderArea(area);

  draw_ctx.dcb.beginRenderPass(begin_rp, vk::SubpassContents::eInline);

  ds.storage.buffer_memcpy(ds.ctx, ubo[frame], 0, &data, sizeof(data));
  draw_ctx.dcb.bindPipeline(vk::PipelineBindPoint::eGraphics, ds.pipelines.get(pipeline));
    
  auto buffers = { scene.get_verts_buff()->api_buffer() };
  auto offsets = {0ul};
    
  draw_ctx.dcb.bindVertexBuffers(0, buffers, offsets);
  draw_ctx.dcb.bindIndexBuffer(scene.get_index_buff()->api_buffer(), 0, vk::IndexType::eUint32);

  auto bind_sets = { ds.descriptors.get(sets[frame]), ds.descriptors.get(texture_set) };
  draw_ctx.dcb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, bind_sets, {});

  const auto& objects = scene.get_objects(); 
  auto &materials = scene.get_materials();
  for (auto &obj : objects) {
    if (materials[obj.material_index].albedo_path.empty()) continue;
    u32 indexes[2] {obj.matrix_index, obj.material_index};
    draw_ctx.dcb.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment, 0u, 2*sizeof(u32), indexes);
    draw_ctx.dcb.drawIndexed(obj.index_count, 1, obj.index_offset, obj.vertex_offset, 0);
  }

  draw_ctx.dcb.endRenderPass();    
}