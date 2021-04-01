#ifndef LIGHTFIELD_PROBES_HPP_INCLUDED
#define LIGHTFIELD_PROBES_HPP_INCLUDED

#include "driverstate.hpp"
#include "scene.hpp"
#include "postprocessing.hpp"

struct LightFieldProbe {
  glm::vec3 pos;
};

struct LightField {
  void init(DriverState &ds);
  void release(DriverState &ds);
  void render(DriverState &ds, Scene &scene, glm::vec3 bmin, glm::vec3 bmax, glm::uvec3 d);
  
  glm::uvec3 get_dimensions() const { return dim; }
  glm::vec3 get_bbox_min() const { return bmin; }
  glm::vec3 get_bbox_max() const { return bmax; }
  glm::vec3 get_probes_start() const { return probe_start; }
  glm::vec3 get_probes_step() const { return probe_step; } 

  std::vector<LightFieldProbe> get_probes() { return probes; }
  drv::ImageViewID &get_distance_array() { return dist_array; }
  drv::ImageViewID &get_normal_array() { return norm_array; }
  drv::ImageViewID &get_lowres_array() { return low_res_array; }
  drv::ImageViewID &get_radiance_array() { return radiance_array; }
  
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
  
  void init_compute_resources(DriverState &ds);
  void downsample_distances(DriverState &ds);

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
  drv::DescriptorSetLayoutID resource_desc;
  drv::DescriptorSetID resource_set;

  drv::BufferID ubo, lights_ubo;

  //render to probe resources
  PostProcessingPass<Nil, Nil> lightprobe_pass;
  
  //Low res filtering
  drv::ComputePipelineID low_res_pipeline;
  vk::PipelineLayout low_res_pipeline_layout;
  drv::DescriptorSetLayoutID low_res_desc_layout;
  drv::DescriptorSetID low_res_bindings;

  glm::uvec3 dim;
  glm::vec3 bmin, bmax;
  glm::vec3 probe_start, probe_step;
  drv::ImageViewID dist_array, norm_array, low_res_array, radiance_array;
  std::vector<LightFieldProbe> probes;

  static constexpr u32 MAX_LIGHTS = 4;

  struct LightSourceData {
    glm::vec4 lights_count;
    glm::vec4 position[MAX_LIGHTS];
    glm::vec4 radiance[MAX_LIGHTS];
  };
};


#endif