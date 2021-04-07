#ifndef SPHERICAL_HARMONICS_HPP_INCLUDED
#define SPHERICAL_HARMONICS_HPP_INCLUDED

#include "driverstate.hpp"

struct SHPass {

  SHPass(DriverState &ds);
  ~SHPass();

  drv::BufferID integrate(DriverState &ds, drv::ImageViewID &image, vk::Sampler sampler);

  static constexpr u32 NUM_SAMPLES = 256;
  static constexpr u32 NUM_SH_COEFFS = 36;
  static constexpr u32 NUM_BANDS = 6;


  struct SHSample {
    //glm::vec4 w;
    float w[4];
    float sh[NUM_SH_COEFFS];
    //float _pad[7];  
  };

  struct SHprobe {
    float coeffs[NUM_SH_COEFFS];
  };
  //https://github.com/rlk/sht - Deringing
private:
  void create_samples_buffer(DriverState &ds);
  void init_shader_resources(DriverState &ds);

  drv::ComputePipelineID pipeline;
  drv::DescriptorSetLayoutID resource_layout;
  drv::DescriptorSetID resources;

  drv::BufferID sh_samples;
};

#endif