#ifndef LIGHTFIELD_PROBES_HPP_INCLUDED
#define LIGHTFIELD_PROBES_HPP_INCLUDED

#include "driverstate.hpp"
#include "scene.hpp"
#include "postprocessing.hpp"

struct LightFieldProbe {
  glm::vec3 pos;
  drv::ImageViewID dist, color, norm; 
};

struct LightField {
  void init(DriverState &ds);
  void release(DriverState &ds);
  void render(DriverState &ds, Scene &scene, glm::vec3 center, glm::vec3 step, glm::uvec3 d);
  
private:
  void create_renderpass(DriverState &ds);
  void create_framebuffer(DriverState &ds);
  void create_pipeline_layout(DriverState &ds);
  void create_pipeline(DriverState &ds);
  void calc_matrix(u32 side, vk::Extent2D ext, glm::vec3 pos, glm::mat4 &out);

  void transform_cubemap_layout(vk::CommandBuffer &buf, vk::ImageLayout src, vk::ImageLayout dst);
  void blit_cubemaps(vk::CommandBuffer &buf, u32 side);

  void render_cubemaps(DriverState &ds, Scene &scene, glm::vec3 center);
  void bind_resources(DriverState &ds, Scene &scene);
  
  struct UBOData {
    glm::mat4 camera_proj;
    glm::vec4 camera_origin;
  };

  //render to cubemap resources
  vk::Framebuffer fb;
  drv::ImageViewID dist, color, norm, depth;
  drv::ImageViewID cm_dist, cm_color, cm_norm;
  vk::Sampler sampler;

  vk::RenderPass renderpass;

  drv::PipelineID pipeline;
  vk::PipelineLayout pipeline_layout;
  drv::DesciptorSetLayoutID resource_desc;
  drv::DescriptorSetID resource_set;

  drv::BufferID ubo;

  //render to probe resources
  PostProcessingPass<Nil, Nil> lightprobe_pass;
  

  glm::uvec3 dim;
  std::vector<LightFieldProbe> probes;
};


#endif