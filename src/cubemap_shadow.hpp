#ifndef CUBEMAP_SHADOW_HPP_INCLUDED
#define CUBEMAP_SHADOW_HPP_INCLUDED

#include "driverstate.hpp"
#include "scene.hpp"

#include <optional>
#include <iostream>

struct CubemapShadowRenderer {
  CubemapShadowRenderer() {}

  void init(DriverState &ds);
  void render(DriverState &ds, drv::ImageID &cubemap, const Scene &scene, glm::vec3 pos);
  void release(DriverState &ds);

private:

  struct UBOData {
    glm::mat4 camera_proj;
    glm::vec4 camera_origin;
  };

  void create_renderpass(DriverState &ds);
  void create_pipeline_layout(DriverState &ds);
  void create_pipeline(DriverState &ds);
  void set_shader_input(DriverState &ds, const Scene &scene);

  void calc_matrix(u32 side, vk::Extent2D ext, glm::vec3 pos, glm::mat4 &out);
  drv::ImageViewID create_side_view(DriverState &ds, u32 side, drv::ImageID &cubemap);

  vk::RenderPass renderpass;

  drv::PipelineID pipeline;
  drv::DesciptorSetLayoutID shader_input;
  drv::DescriptorSetID shader_res;
  vk::PipelineLayout pipeline_layout;

  drv::BufferID ubo;
};

#endif