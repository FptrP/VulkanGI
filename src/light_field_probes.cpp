#include "lightfield_probes.hpp"
#include "drv/cmd_utils.hpp"
#include <iostream>

#include <cstdlib>
#include <cmath>

const u32 CUBEMAP_RES = 512;
const vk::Extent2D CUBEMAP_EXT {CUBEMAP_RES, CUBEMAP_RES};
const u32 OCT_RES = 1024;
const u32 DIST_MIPS = 7;

void LightField::calc_matrix(u32 side, vk::Extent2D ext, glm::vec3 pos, glm::mat4 &out) {
  float aspect = float(ext.width)/float(ext.height);

  glm::mat4 proj = glm::perspective(glm::radians(90.f), aspect, 0.01f, 100.f);

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
    .add_storage_buffer(1, vk::ShaderStageFlagBits::eVertex)
    .add_array_of_tex(2, 25, vk::ShaderStageFlagBits::eFragment)
    .add_sampler(3, vk::ShaderStageFlagBits::eFragment)
    .add_ubo(4, vk::ShaderStageFlagBits::eFragment)
    .add_combined_sampler(5, vk::ShaderStageFlagBits::eFragment);
  
  resource_desc = ds.descriptors.create_layout(ds.ctx, builder.build(), 1);
  resource_set = ds.descriptors.allocate_set(ds.ctx, resource_desc);

  vk::PushConstantRange range {};
  range.setOffset(0);
  range.setSize(2 * sizeof(u32));
  range.setStageFlags(vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment);

  auto set_layouts = { ds.descriptors.get(resource_desc) };
  auto ranges = { range };

  vk::PipelineLayoutCreateInfo info {};
  info
    .setSetLayouts(set_layouts)
    .setPushConstantRanges(ranges);
  
  pipeline_layout = ds.ctx.get_device().createPipelineLayout(info);

  ubo = ds.storage.create_buffer(ds.ctx, drv::GPUMemoryT::Coherent, sizeof(UBOData), vk::BufferUsageFlagBits::eUniformBuffer);
  lights_ubo = ds.storage.create_buffer(ds.ctx, drv::GPUMemoryT::Coherent, sizeof(LightSourceData), vk::BufferUsageFlagBits::eUniformBuffer);
}

void LightField::create_pipeline(DriverState &ds) {
  ds.pipelines.load_shader(ds.ctx, "cube_probe_vs", "src/shaders/cube_probe_vert.spv", vk::ShaderStageFlagBits::eVertex);
  ds.pipelines.load_shader(ds.ctx, "cube_probe_fs", "src/shaders/cube_probe_frag.spv", vk::ShaderStageFlagBits::eFragment);

  drv::PipelineDescBuilder builder {};
  builder
    .add_shader("cube_probe_vs")
    .add_shader("cube_probe_fs")
    
    .add_attribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(SceneVertex, pos))
    .add_attribute(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(SceneVertex, norm))
    .add_attribute(2, 0, vk::Format::eR32G32Sfloat, offsetof(SceneVertex, uv))
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
  ds.pipelines.load_shader(ds.ctx, "cube_probe_to_oct_depth_fs", "src/shaders/cube_probe_to_oct_depth_frag.spv", vk::ShaderStageFlagBits::eFragment);

  lightprobe_pass
    .init_attachment(vk::Format::eR32Sfloat) //distance
    .init_attachment(vk::Format::eR16G16B16A16Sfloat) //normal
    .init_attachment(vk::Format::eR16G16B16A16Sfloat) //radiance
  #if (OCT_DEPTH)  
    .init(ds, 3, "cube_probe_to_oct_depth_fs");
  #else
    .init(ds, 3, "cube_probe_to_oct_fs");
  #endif
  init_compute_resources(ds);
  init_hidist_resources(ds);
}

void LightField::release(DriverState &ds) {
  ds.ctx.get_device().destroySampler(hidist_pass.nearest_sampler);
  lightprobe_pass.release(ds);
  ds.ctx.get_device().destroySampler(sampler);
  ds.pipelines.free_pipeline(ds.ctx, pipeline);
  //ds.ctx.get_device().destroyPipelineLayout(pipeline_layout);
  ds.ctx.get_device().destroyFramebuffer(fb);
  ds.ctx.get_device().destroyRenderPass(renderpass);

}

void LightField::render_cubemaps(DriverState &ds, Scene &scene, glm::vec3 center) {
  for (u32 side = 0; side < 6; side++) {
    UBOData data;
    data.camera_origin = glm::vec4{center.x, center.y, center.z, 0.f};
    calc_matrix(side, vk::Extent2D{CUBEMAP_RES, CUBEMAP_RES}, center, data.camera_proj);
    ds.storage.buffer_memcpy(ds.ctx, ubo, 0, &data, sizeof(data));

    {
      LightSourceData data;
      data.lights_count.x = scene.get_lights().size();
      const auto &scene_lights = scene.get_lights();

      for (u32 i = 0; i < min(MAX_LIGHTS, u32(scene_lights.size())); i++) {
        data.position[i] = glm::vec4{scene_lights[i].position, 0.f};
        data.radiance[i] = glm::vec4{scene_lights[i].color, 0.f};
      }

      ds.storage.buffer_memcpy(ds.ctx, lights_ubo, 0, &data, sizeof(data));
    }

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
    auto &tex_info = scene.get_materials();
    
    for (auto &obj : objects) {
      auto albedo_id = tex_info.materials[obj.material_index].albedo_tex_id;
      
      if (albedo_id < 0) continue;

      u32 pc_data[2];
      pc_data[0] = obj.matrix_index;
      pc_data[1] = albedo_id; 
      cmd.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment, 0u, 2*sizeof(u32), pc_data);
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
  std::vector<vk::ImageView> api_views;
  for (auto &view : scene.get_materials().albedo_images) {
    api_views.push_back(view->api_view());
  }

  drv::DescriptorBinder bind {ds.descriptors.get(resource_set)};
  bind
    .bind_ubo(0, ubo->api_buffer())
    .bind_storage_buff(1, scene.get_matrix_buff()->api_buffer())
    .bind_array_of_img(2, api_views.size(), api_views.data())
    .bind_sampler(3, sampler)
    .bind_ubo(4, lights_ubo->api_buffer())
    .bind_combined_img(5, scene.get_shadows_array()->api_view(), sampler);
  
  bind.write(ds.ctx);
}

void LightField::render(DriverState &ds, Scene &scene, glm::vec3 bmin, glm::vec3 bmax, glm::uvec3 d) {
  auto ARR_USG = vk::ImageUsageFlagBits::eColorAttachment|vk::ImageUsageFlagBits::eSampled;
  auto layers = d.x * d.y * d.z;
  auto dist_img = ds.storage.create_image2D_array(ds.ctx, OCT_RES, OCT_RES, vk::Format::eR32Sfloat, ARR_USG|vk::ImageUsageFlagBits::eStorage, layers, DIST_MIPS);
  dist_array = ds.storage.create_2Darray_view(ds.ctx, dist_img, vk::ImageAspectFlagBits::eColor, true);

  auto norm_img = ds.storage.create_image2D_array(ds.ctx, OCT_RES, OCT_RES, vk::Format::eR16G16B16A16Sfloat, ARR_USG, layers);
  norm_array = ds.storage.create_2Darray_view(ds.ctx, norm_img, vk::ImageAspectFlagBits::eColor);

  auto radiance_img = ds.storage.create_image2D_array(ds.ctx, OCT_RES, OCT_RES, vk::Format::eR16G16B16A16Sfloat, ARR_USG, layers);
  radiance_array = ds.storage.create_2Darray_view(ds.ctx, radiance_img, vk::ImageAspectFlagBits::eColor);

  dim = d;
  this->bmax = bmax;
  this->bmin = bmin;
  bind_resources(ds, scene);
  probes.reserve(d.x * d.y * d.z);

  glm::vec3 cell_size = (bmax - bmin)/glm::vec3{d};
  glm::vec3 cell_offset = 0.5f * cell_size;
  
  probe_start = bmin + cell_offset; 
  probe_step = cell_size;
  
  for (u32 x = 0; x < d.x; x++) {
    for (u32 y = 0; y < d.y; y++) {
      for (u32 z = 0; z < d.z; z++) {
        glm::vec3 pos = bmin + glm::vec3{cell_size.x * x, cell_size.y * y, cell_size.z * z} + cell_offset;
        std::cout << pos.x << " " << pos.y << " " << pos.z << "\n";
        render_cubemaps(ds, scene, pos);
        
        LightFieldProbe probe;
        probe.pos = pos;

        u32 index = probes.size();
        auto layer_view = ds.storage.create_2Dlayer_view(ds.ctx, dist_img, vk::ImageAspectFlagBits::eColor, index);
        auto norm_view = ds.storage.create_2Dlayer_view(ds.ctx, norm_img, vk::ImageAspectFlagBits::eColor, index);
        auto radiance_view = ds.storage.create_2Dlayer_view(ds.ctx, radiance_img, vk::ImageAspectFlagBits::eColor, index);

        lightprobe_pass.set_render_area(OCT_RES, OCT_RES);
        lightprobe_pass.set_attachment(0, layer_view);
        lightprobe_pass.set_attachment(1, norm_view);
        lightprobe_pass.set_attachment(2, radiance_view);
        lightprobe_pass.set_image_sampler(0, cm_dist, sampler);
        lightprobe_pass.set_image_sampler(1, cm_color, sampler);
        lightprobe_pass.set_image_sampler(2, cm_norm, sampler);
        std::cout << "Filling probe\n";
        lightprobe_pass.render_and_wait(ds);
        std::cout << "End\n";
        probes.push_back(probe);
      }
    }
  }
  std::cout << "Downsampling distances\n";
  downsample_distances(ds);
  std::cout << "End\n";
  std::cout << "Irradiance\n";
  compute_irradiance(ds);
  std::cout << "End\n";
  create_hidist_images(ds);
  hidist_array = ds.storage.create_2Darray_view(ds.ctx, dist_img, vk::ImageAspectFlagBits::eColor, false);
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

void LightField::init_compute_resources(DriverState &ds) {
  ds.pipelines.load_shader(ds.ctx, "oct_fold_cs", "src/shaders/oct_fold_comp.spv", vk::ShaderStageFlagBits::eCompute);
  ds.pipelines.load_shader(ds.ctx, "irradiance_cs", "src/shaders/irradiance_comp.spv", vk::ShaderStageFlagBits::eCompute);
  {
    drv::DescriptorSetLayoutBuilder layout_builder {};
    layout_builder
      .add_combined_sampler(0, vk::ShaderStageFlagBits::eCompute)
      .add_storage_image(1, vk::ShaderStageFlagBits::eCompute);

    low_res_desc_layout = ds.descriptors.create_layout(ds.ctx, layout_builder.build(), 1);

    auto layouts = {ds.descriptors.get(low_res_desc_layout)};

    vk::PipelineLayoutCreateInfo layout_info {};
    layout_info.setSetLayouts(layouts);

    low_res_pipeline_layout = ds.ctx.get_device().createPipelineLayout(layout_info);
    low_res_pipeline = ds.pipelines.create_compute_pipeline(ds.ctx, "oct_fold_cs", low_res_pipeline_layout);

    low_res_bindings = ds.descriptors.allocate_set(ds.ctx, low_res_desc_layout);
  }

  {
    drv::DescriptorSetLayoutBuilder playout_builder{};
    playout_builder
      .add_combined_sampler(0, vk::ShaderStageFlagBits::eCompute)
      .add_ubo(1, vk::ShaderStageFlagBits::eCompute)
      .add_storage_image(2, vk::ShaderStageFlagBits::eCompute);

    irradiance_pass.descriptor_layout = ds.descriptors.create_layout(ds.ctx, playout_builder.build(), 1);

    auto layouts = {ds.descriptors.get(irradiance_pass.descriptor_layout)};

    vk::PipelineLayoutCreateInfo layout_info {};
    layout_info.setSetLayouts(layouts);

    irradiance_pass.pipeline_layout = ds.ctx.get_device().createPipelineLayout(layout_info);
    irradiance_pass.pipeline = ds.pipelines.create_compute_pipeline(ds.ctx, "irradiance_cs", irradiance_pass.pipeline_layout);

    irradiance_pass.descriptor = ds.descriptors.allocate_set(ds.ctx, irradiance_pass.descriptor_layout);
    irradiance_pass.samples_buffer 
      = ds.storage.create_buffer(ds.ctx, drv::GPUMemoryT::Coherent, sizeof(IrradianceSamples), vk::BufferUsageFlagBits::eUniformBuffer);

    IrradianceSamples *ptr = (IrradianceSamples*)ds.storage.map_buffer(ds.ctx, irradiance_pass.samples_buffer);

    for (u32 i = 0; i < SAMPLES_COUNT; i++) {
      float x = 2.f * std::rand()/float(RAND_MAX) - 1.f;
      float y = 2.f * std::rand()/float(RAND_MAX) - 1.f;
      float z = 2.f * std::rand()/float(RAND_MAX) - 1.f;

      float dist = std::max(std::sqrt(x*x + y*y+ z*z), 1e-6f);
      ptr->positions[i].x = x/dist;
      ptr->positions[i].y = y/dist;
      ptr->positions[i].z = z/dist;
    }
    ds.storage.unmap_buffer(ds.ctx, irradiance_pass.samples_buffer);
  }
}

void LightField::init_hidist_resources(DriverState &ds) {
  ds.pipelines.load_shader(ds.ctx, "hi_dist_mips_cs", "src/shaders/hi_dist_mips_comp.spv", vk::ShaderStageFlagBits::eCompute);

  drv::DescriptorSetLayoutBuilder builder{};
  builder
    .add_combined_sampler(0, vk::ShaderStageFlagBits::eCompute)
    .add_storage_image(1, vk::ShaderStageFlagBits::eCompute);

  hidist_pass.descriptor_layout = ds.descriptors.create_layout(ds.ctx, builder.build(), DIST_MIPS);

  auto used_layouts = {ds.descriptors.get(hidist_pass.descriptor_layout)};

  vk::PipelineLayoutCreateInfo info {};
  info.setSetLayouts(used_layouts);

  auto pipeline_layout = ds.ctx.get_device().createPipelineLayout(info);
  hidist_pass.pipeline = ds.pipelines.create_compute_pipeline(ds.ctx, "hi_dist_mips_cs", pipeline_layout);


  vk::SamplerCreateInfo sinfo {};
  sinfo
    .setMinFilter(vk::Filter::eNearest)
    .setMagFilter(vk::Filter::eNearest)
    .setMaxLod(1000.f)
    .setMinLod(0.f)
    .setMipmapMode(vk::SamplerMipmapMode::eNearest);

  hidist_pass.nearest_sampler = ds.ctx.get_device().createSampler(sinfo);
}

void LightField::downsample_distances(DriverState &ds) {
  auto layers = dim.x * dim.y * dim.z;

  auto low_res_img = ds.storage.create_image2D_array(ds.ctx, 64, 64, vk::Format::eR32Sfloat, vk::ImageUsageFlagBits::eStorage|vk::ImageUsageFlagBits::eSampled, layers);
  low_res_array = ds.storage.create_2Darray_view(ds.ctx, low_res_img, vk::ImageAspectFlagBits::eColor);

  drv::DescriptorBinder binder {ds.descriptors.get(low_res_bindings)};
  binder
    .bind_combined_img(0, dist_array->api_view(), sampler)
    .bind_storage_image(1, low_res_array->api_view())
    .write(ds.ctx);


  auto cmd = ds.submit_pool.start_cmd(ds.ctx);
  vk::CommandBufferBeginInfo begin_info {};
  cmd.begin(begin_info);
  
  drv::ImageBarrier to_general{low_res_img, vk::ImageAspectFlagBits::eColor};

  to_general.set_range(0, 1, 0, layers);
  to_general.change_layout(vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
  to_general.access_msk(vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eMemoryWrite);
  to_general.write(cmd, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eComputeShader);

  cmd.bindPipeline(vk::PipelineBindPoint::eCompute, ds.pipelines.get(low_res_pipeline));
  cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, ds.pipelines.get_layout(low_res_pipeline), 0, {ds.descriptors.get(low_res_bindings)}, {});
  cmd.dispatch(4, 4, layers);

  drv::ImageBarrier to_sampled{low_res_img, vk::ImageAspectFlagBits::eColor};
  to_sampled.set_range(0, 1, 0, layers);
  to_sampled.change_layout(vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal);
  to_sampled.access_msk(vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eMemoryRead);
  to_sampled.write(cmd, vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eBottomOfPipe);


  cmd.end();
  auto fence = ds.submit_pool.submit_cmd(ds.ctx, cmd);
  ds.ctx.get_device().waitForFences({fence}, VK_TRUE, ~(0ul));
  ds.ctx.get_device().destroyFence(fence);
}

void LightField::compute_irradiance(DriverState &ds) {
  auto layers = dim.x * dim.y * dim.z;
  auto irradiance_img = ds.storage.create_image2D_array(ds.ctx, 64, 64, vk::Format::eR16G16B16A16Sfloat, vk::ImageUsageFlagBits::eStorage|vk::ImageUsageFlagBits::eSampled, layers);

  irradiance_pass.image_view = ds.storage.create_2Darray_view(ds.ctx, irradiance_img, vk::ImageAspectFlagBits::eColor);

  drv::DescriptorBinder binder {ds.descriptors.get(irradiance_pass.descriptor)};
  binder
    .bind_combined_img(0, radiance_array->api_view(), sampler)
    .bind_storage_image(2, irradiance_pass.image_view->api_view())
    .bind_ubo(1, irradiance_pass.samples_buffer->api_buffer())
    .write(ds.ctx);
  
  auto cmd = ds.submit_pool.start_cmd(ds.ctx);
  vk::CommandBufferBeginInfo begin_info {};
  cmd.begin(begin_info);
  
  drv::ImageBarrier to_general{irradiance_img, vk::ImageAspectFlagBits::eColor};

  to_general.set_range(0, 1, 0, layers);
  to_general.change_layout(vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
  to_general.access_msk(vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eMemoryWrite);
  to_general.write(cmd, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eComputeShader);

  cmd.bindPipeline(vk::PipelineBindPoint::eCompute, ds.pipelines.get(irradiance_pass.pipeline));
  cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, ds.pipelines.get_layout(irradiance_pass.pipeline), 0, {ds.descriptors.get(irradiance_pass.descriptor)}, {});
  cmd.dispatch(8, 16, layers);

  drv::ImageBarrier to_sampled{irradiance_img, vk::ImageAspectFlagBits::eColor};
  to_sampled.set_range(0, 1, 0, layers);
  to_sampled.change_layout(vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal);
  to_sampled.access_msk(vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eMemoryRead);
  to_sampled.write(cmd, vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eBottomOfPipe);


  cmd.end();
  auto fence = ds.submit_pool.submit_cmd(ds.ctx, cmd);
  ds.ctx.get_device().waitForFences({fence}, VK_TRUE, ~(0ul));
  ds.ctx.get_device().destroyFence(fence);
}

void LightField::create_hidist_images(DriverState &ds) {
  std::cout << "HiDist started\n";
  auto layers = dim.x * dim.y * dim.z;

  std::vector<VkSampler> samplers;
  
  auto cmd = ds.submit_pool.start_cmd(ds.ctx);
  vk::CommandBufferBeginInfo begin_info {};
  cmd.begin(begin_info);

  auto src_view = ds.storage.create_2Darray_mip_view(ds.ctx, dist_array->get_base_img(), vk::ImageAspectFlagBits::eColor, 0);

  cmd.bindPipeline(vk::PipelineBindPoint::eCompute, ds.pipelines.get(hidist_pass.pipeline));

  std::vector<drv::DescriptorSetID> desc;

  u32 resolution = OCT_RES/2;

  for (u32 mip_level = 0; mip_level < DIST_MIPS - 1; mip_level++) {
    vk::PipelineStageFlags src_flags = (mip_level == 0)? vk::PipelineStageFlagBits::eTopOfPipe : vk::PipelineStageFlagBits::eComputeShader;
    vk::PipelineStageFlags dst_flags = (mip_level == (DIST_MIPS - 2))? vk::PipelineStageFlagBits::eBottomOfPipe : vk::PipelineStageFlagBits::eComputeShader;
    //uto img_layout = (mip_level == )
    auto dst_view = ds.storage.create_2Darray_mip_view(ds.ctx, dist_array->get_base_img(), vk::ImageAspectFlagBits::eColor, mip_level + 1);
    
    drv::ImageBarrier to_general{dist_array->get_base_img(), vk::ImageAspectFlagBits::eColor};
    to_general
      .set_range(mip_level + 1, 1, 0, layers)
      .change_layout(vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral)
      .access_msk(vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eMemoryWrite)
      .write(cmd, src_flags, vk::PipelineStageFlagBits::eComputeShader);

    auto descriptor = ds.descriptors.allocate_set(ds.ctx, hidist_pass.descriptor_layout);
    drv::DescriptorBinder binder{ds.descriptors.get(descriptor)};
    binder
      .bind_combined_img(0, src_view->api_view(), hidist_pass.nearest_sampler)
      .bind_storage_image(1, dst_view->api_view())
      .write(ds.ctx);

    desc.push_back(descriptor);

    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, ds.pipelines.get_layout(hidist_pass.pipeline), 0, {ds.descriptors.get(descriptor)}, {});
    cmd.dispatch(resolution/8, resolution/4, layers);
    resolution /= 2;

    drv::ImageBarrier to_sampled{dist_array->get_base_img(), vk::ImageAspectFlagBits::eColor};
    to_general
      .set_range(mip_level + 1, 1, 0, layers)
      .change_layout(vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal)
      .access_msk(vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eMemoryWrite)
      .write(cmd, src_flags, vk::PipelineStageFlagBits::eComputeShader);

    src_view = dst_view;
  }

  cmd.end();
  auto fence = ds.submit_pool.submit_cmd(ds.ctx, cmd);
  ds.ctx.get_device().waitForFences({fence}, VK_TRUE, ~(0ul));
  ds.ctx.get_device().destroyFence(fence);

  std::cout << "HiDist created\n";
}