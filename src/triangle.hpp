#ifndef TRIANGLE_HPP_INCLUDED
#define TRIANGLE_HPP_INCLUDED

#include "frame_global.hpp"
#include "scene.hpp"
#include "cubemap_shadow.hpp"
#include "render_oct.hpp"

#include <optional>
#include <iostream>

struct GBufferSubpass {
  GBufferSubpass(FrameGlobal &fg) : frame_data{fg} {}

  void init(DriverState &ds) {
    create_renderpass(ds);
    frame_data.get_gbuffer().init(ds, gbuf_renderpass);
    create_framebuffer(ds);
    create_pipeline_layout(ds);
    create_pipeline(ds);
  }

  void release(DriverState &ds) {
    frame_data.get_gbuffer().release(ds);
    ds.pipelines.free_pipeline(ds.ctx, pipeline);
    ds.ctx.get_device().destroyFramebuffer(framebuf);
    ds.ctx.get_device().destroyRenderPass(gbuf_renderpass);
    ds.descriptors.free_layout(ds.ctx, desc_layout);
    ds.ctx.get_device().destroySampler(sampler);
  }

  void update(float dt) {

  }

  void render(drv::DrawContext &draw_ctx, DriverState &ds);

private:
  void create_texture_sets(DriverState &ds);
  void create_renderpass(DriverState &ds);
  void create_framebuffer(DriverState &ds);  
  void create_pipeline(DriverState &ds);
  void create_pipeline_layout(DriverState &ds);

  struct VertexUB {
    glm::mat4 camera;
    glm::mat4 inv_camera;
    glm::mat4 project;
    glm::vec4 camera_origin;
  };

  drv::PipelineID pipeline;
  drv::DescriptorSetLayoutID desc_layout, tex_layout;
  vk::PipelineLayout pipeline_layout;

  std::vector<drv::DescriptorSetID> sets;
  drv::DescriptorSetID texture_set; 

  drv::BufferID ubo[drv::MAX_FRAMES_IN_FLIGHT];
  vk::Sampler sampler;

  FrameGlobal &frame_data;

  vk::RenderPass gbuf_renderpass;
  vk::Framebuffer framebuf;
};

#endif