#ifndef SHADING_HPP_INCLUDED
#define SHADING_HPP_INCLUDED

#include "driverstate.hpp"
#include "scene.hpp"
#include "postprocessing.hpp"

#include <optional>
#include <iostream>


struct ShadingPass {

  ShadingPass(DriverState &ds, FrameGlobal &frame) : frame_data {frame} {
    create_shader_desc(ds, frame);
    create_pipeline(ds);
  }

  ~ShadingPass() {}

  void release(DriverState &ds) {
    ds.descriptors.free_layout(ds.ctx, tex_layout);
  }

  void create_shader_desc(DriverState &ds, FrameGlobal &frame) {
    drv::DescriptorSetLayoutBuilder tex {};
    tex
      .add_combined_sampler(0, vk::ShaderStageFlagBits::eFragment)
      .add_combined_sampler(1, vk::ShaderStageFlagBits::eFragment)
      .add_combined_sampler(2, vk::ShaderStageFlagBits::eFragment)
      .add_combined_sampler(3, vk::ShaderStageFlagBits::eFragment)
      .add_combined_sampler(4, vk::ShaderStageFlagBits::eFragment);
    
    tex_layout = ds.descriptors.create_layout(ds.ctx, tex.build(), 1);
    auto tex_set = ds.descriptors.allocate_set(ds.ctx, tex_layout);
    
    auto& gbuff = frame.get_gbuffer();

    drv::DescriptorBinder binder {ds.descriptors.get(tex_set)};
    binder
      .bind_combined_img(0, gbuff.images[0]->api_view(), gbuff.sampler)
      .bind_combined_img(1, gbuff.images[1]->api_view(), gbuff.sampler)
      .bind_combined_img(2, gbuff.images[2]->api_view(), gbuff.sampler)
      .bind_combined_img(3, gbuff.images[3]->api_view(), gbuff.sampler)
      .bind_combined_img(4, frame_data.get_cubemap()->api_view(), gbuff.sampler);
      
    binder.write(ds.ctx);
    sets.push_back(tex_set);
  }

  void create_pipeline(DriverState &ds) {
    //ds.pipelines.load_shader(ds.ctx, "pass_vs", "src/shaders/pass_vert.spv", vk::ShaderStageFlagBits::eVertex);
    //ds.pipelines.load_shader(ds.ctx, "shading_fs", "src/shaders/shading_frag.spv", vk::ShaderStageFlagBits::eFragment);

    auto layouts = {ds.descriptors.get(tex_layout)};
    
    vk::PushConstantRange push {};
    push
      .setOffset(0)
      .setSize(sizeof(glm::vec3))
      .setStageFlags(vk::ShaderStageFlagBits::eFragment);

    auto push_ranges = {push};

    vk::PipelineLayoutCreateInfo linfo {};
    linfo
      .setPushConstantRanges(push_ranges)
      .setSetLayouts(layouts);
    
    pipeline_layout = ds.ctx.get_device().createPipelineLayout(linfo);
    auto ext = ds.ctx.get_swapchain_extent();

    drv::PipelineDescBuilder builder {};
    builder
      .add_shader("pass_vs")
      .add_shader("shading_fs")
    
      .set_vertex_assembly(vk::PrimitiveTopology::eTriangleList, false)
      .set_polygon_mode(vk::PolygonMode::eFill)
      .set_front_face(vk::FrontFace::eCounterClockwise)

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

    pipeline = ds.pipelines.create_pipeline(ds.ctx, builder);
  }

  void render(drv::DrawContext &draw_ctx, DriverState &ds) {

    auto pos = frame_data.get_camera_pos();

    draw_ctx.dcb.bindPipeline(vk::PipelineBindPoint::eGraphics, ds.pipelines.get(pipeline));
    auto desc_sets = {ds.descriptors.get(sets[0])}; 
    draw_ctx.dcb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, desc_sets, {});
    draw_ctx.dcb.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(glm::vec3), &pos);
    draw_ctx.dcb.draw(3, 1, 0, 0);
  }

private:
  drv::PipelineID pipeline;
  drv::DesciptorSetLayoutID tex_layout;
  vk::PipelineLayout pipeline_layout;
  std::vector<drv::DescriptorSetID> sets;

  //drv::BufferID ubo[drv::MAX_FRAMES_IN_FLIGHT];
  FrameGlobal &frame_data;
};

#endif