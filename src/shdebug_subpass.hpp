#ifndef SHDEBUG_SUBPASS_HPP_INCLUDED
#define SHDEBUG_SUBPASS_HPP_INCLUDED

#include "frame_global.hpp"

#include "drv/imgui_context.hpp"

struct SHDebugSubpass {
  SHDebugSubpass(DriverState &ds, FrameGlobal &frame) : frame_data{frame} {
    init_pipeline(ds);
  }

  ~SHDebugSubpass() {

  }

  void render(drv::DrawContext &draw_ctx, DriverState &ds) {
    int layers = int(frame_data.get_light_field().get_lowres_array()->get_base_img()->get_info().arrayLayers);

    {
      ImGui::Begin("SHDebug settings");
      static int layer = 0; 
      ImGui::SliderInt("Probe id", &layer, 0, layers - 1);
      if (ImGui::Button("ShowTex")) {
        settings.flags = (settings.flags + 1) % 3;
      }

      settings.probe_id = u32(layer);    

      ImGui::End();
    }


    u32 frame_id = draw_ctx.frame_id;

    glm::mat4 mvp = frame_data.get_camera_matrix();
    mvp[3][0] = mvp[3][1] = mvp[3][2] = 0;
    mvp = frame_data.get_projection_matrix() * mvp;

    ds.storage.buffer_memcpy(ds.ctx, ubo[frame_id], 0, &mvp, sizeof(mvp));

    auto desc_sets = {ds.descriptors.get(resources[frame_id])};
    u32 render_flags = 0;

    draw_ctx.dcb.bindPipeline(vk::PipelineBindPoint::eGraphics, ds.pipelines.get(pipeline));
    draw_ctx.dcb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, ds.pipelines.get_layout(pipeline), 0, desc_sets, {});
    draw_ctx.dcb.pushConstants(ds.pipelines.get_layout(pipeline), vk::ShaderStageFlagBits::eFragment, 0, sizeof(settings), &settings);
    draw_ctx.dcb.draw(36, 1, 0, 0);
  }


private:

  void init_pipeline(DriverState &ds) {
    ds.pipelines.load_shader(ds.ctx, "cubemap_verts_vs", "src/shaders/cubemap_verts_vert.spv", vk::ShaderStageFlagBits::eVertex);
    ds.pipelines.load_shader(ds.ctx, "sh_draw_fs", "src/shaders/sh_draw_frag.spv", vk::ShaderStageFlagBits::eFragment);

    drv::DescriptorSetLayoutBuilder builder {};
    builder
      .add_ubo(0, vk::ShaderStageFlagBits::eVertex)
      .add_storage_buffer(1, vk::ShaderStageFlagBits::eFragment)
      .add_combined_sampler(2, vk::ShaderStageFlagBits::eFragment);
    
    resource_layout = ds.descriptors.create_layout(ds.ctx, builder.build(), drv::MAX_FRAMES_IN_FLIGHT);

    vk::PushConstantRange range {};
    range.setSize(sizeof(PushData));
    range.setStageFlags(vk::ShaderStageFlagBits::eFragment);

    auto used_layouts = {ds.descriptors.get(resource_layout)};
    auto used_ranges = {range};

    vk::PipelineLayoutCreateInfo layout_info {};
    layout_info.setSetLayouts(used_layouts);
    layout_info.setPushConstantRanges(used_ranges);

    auto pipeline_layout = ds.ctx.get_device().createPipelineLayout(layout_info);
    auto ext = ds.ctx.get_swapchain_extent();

    drv::PipelineDescBuilder pbuilder{};
    pbuilder
      .add_shader("cubemap_verts_vs")
      .add_shader("sh_draw_fs")
    
      .set_vertex_assembly(vk::PrimitiveTopology::eTriangleList, false)
      .set_polygon_mode(vk::PolygonMode::eFill)
      .set_front_face(vk::FrontFace::eCounterClockwise)
      .set_cull_mode(vk::CullModeFlagBits::eNone)

      .add_viewport(0.f, 0.f, ext.width, ext.height, 0.f, 1.f)
      .add_scissors(0, 0, ext.width, ext.height)

      .set_blend_logic_op(false)
      .add_blend_attachment()
      .set_blend_constants(1.f, 1.f, 1.f, 1.f)

      .set_depth_test(false)
      .set_depth_func(vk::CompareOp::eNever)
      .set_depth_write(false)

      .attach_to_renderpass(ds.main_renderpass, 0)
      .set_layout(pipeline_layout);

    pipeline = ds.pipelines.create_pipeline(ds.ctx, pbuilder);
    
    for (u32 i = 0; i < drv::MAX_FRAMES_IN_FLIGHT; i++) {
      ubo[i] = ds.storage.create_buffer(ds.ctx, drv::GPUMemoryT::Coherent, sizeof(UBOData), vk::BufferUsageFlagBits::eUniformBuffer);
      resources[i] = ds.descriptors.allocate_set(ds.ctx, resource_layout);

      drv::DescriptorBinder binder{ds.descriptors.get(resources[i])};
      binder
        .bind_ubo(0, ubo[i]->api_buffer())
        .bind_storage_buff(1, frame_data.get_sh_probes()->api_buffer())
        .bind_combined_img(2, frame_data.get_light_field().get_distance_array()->api_view(), frame_data.get_default_sampler())
        .write(ds.ctx);
    }
  }
  
  FrameGlobal &frame_data;

  drv::PipelineID pipeline;
  drv::DescriptorSetLayoutID resource_layout;
  drv::DescriptorSetID resources[drv::MAX_FRAMES_IN_FLIGHT];

  struct UBOData {
    glm::mat4 mvp;
  };

  struct PushData {
    u32 flags = 0;
    u32 probe_id = 0;
  };
  
  PushData settings {};

  drv::BufferID ubo[drv::MAX_FRAMES_IN_FLIGHT];
};

#endif