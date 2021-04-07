#include "shpherical_harmonics.hpp"

#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdlib>

#include <iostream>
#include <cassert>
double factorial(uint32_t x) {
  double res = 1;
  for (uint32_t i = 2; i <= x; i++) {
    res *= i;
  }
  //std::cout << "factorial(" << x << ")= " <<res << "\n";
  return res;
}

double K(int l, int m) {
// renormalisation constant for SH function
  double temp = ((2.0*l+1.0)*factorial(l-m)) / (4.0*M_PI*factorial(l+m));
  assert(temp > 0);
  return sqrt(temp);
}

double P(int l,int m,double x) {
// evaluate an Associated Legendre Polynomial P(l,m,x) at x
  double pmm = 1.0;

  if(m>0) {
    assert((1.0-x)*(1.0+x) > 0);
    double somx2 = sqrt((1.0-x)*(1.0+x));
    double fact = 1.0;
    for(int i=1; i<=m; i++) {
      pmm *= (-fact) * somx2;
      fact += 2.0;
    }
  }
  if(l==m) return pmm;
  double pmmp1 = x * (2.0*m+1.0) * pmm;
  if(l==m+1) return pmmp1;
  double pll = 0.0;
  for(int ll=m+2; ll<=l; ++ll) {
    pll = ( (2.0*ll-1.0)*x*pmmp1-(ll+m-1.0)*pmm ) / (ll-m);
    pmm = pmmp1;
    pmmp1 = pll;
  }
  return pll;
}

double SHf(int l, int m, double theta, double phi) {
// return a point sample of a Spherical Harmonic basis function
// l is the band, range [0..N]
// m in the range [-l..l]
// theta in the range [0..Pi]
// phi in the range [0..2*Pi]
  const double sqrt2 = sqrt(2.0);

  if(m==0) return K(l,0)*P(l,m,cos(theta));
  else if(m>0) return sqrt2*K(l,m)*cos(m*phi)*P(l,m,cos(theta));
  else return sqrt2*K(l,-m)*sin(-m*phi)*P(l,-m,cos(theta));
}

static float randf() {
  return (float)random()/((float)RAND_MAX);
}


SHPass::SHPass(DriverState &ds) {
  create_samples_buffer(ds);
  init_shader_resources(ds);
}

SHPass::~SHPass() {

}

drv::BufferID SHPass::integrate(DriverState &ds, drv::ImageViewID &image, vk::Sampler sampler) {
  const auto &img_info = image->get_base_img()->get_info();
  u32 layers = img_info.arrayLayers;

  auto result_buffer = ds.storage.create_buffer(
    ds.ctx, drv::GPUMemoryT::Local, sizeof(SHprobe) * layers, 
    vk::BufferUsageFlagBits::eStorageBuffer|vk::BufferUsageFlagBits::eUniformBuffer);

  drv::DescriptorBinder bind_resources{ds.descriptors.get(resources)};
  bind_resources
    .bind_storage_buff(0, sh_samples->api_buffer())
    .bind_storage_buff(1, result_buffer->api_buffer())
    .bind_combined_img(2, image->api_view(), sampler)
    .write(ds.ctx);

  auto cmd = ds.submit_pool.start_cmd(ds.ctx);
  vk::CommandBufferBeginInfo begin_info {};
  cmd.begin(begin_info);
  
  cmd.bindPipeline(vk::PipelineBindPoint::eCompute, ds.pipelines.get(pipeline));
  cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, ds.pipelines.get_layout(pipeline), 0, {ds.descriptors.get(resources)}, {});
  cmd.dispatch(layers, 1, 1);

  cmd.end();
  auto fence = ds.submit_pool.submit_cmd(ds.ctx, cmd);
  auto res = ds.ctx.get_device().waitForFences({fence}, VK_TRUE, ~(0ul));
  ds.ctx.get_device().destroyFence(fence);

  if (res != vk::Result::eSuccess) {
    throw std::runtime_error {"WaitFence error"};
  }


  return result_buffer;
}

void SHPass::create_samples_buffer(DriverState &ds) {
  std::vector<SHSample> temp_samples;

  temp_samples.reserve(NUM_SAMPLES * NUM_SAMPLES);

  double oneoverN = 1.0/NUM_SAMPLES;
  for (int a = 0; a < int(NUM_SAMPLES); a++) {
    for (int b = 0; b < int(NUM_SAMPLES); b++) {
      SHSample sample {};
      double x = (a + randf()) * oneoverN; // do not reuse results
      double y = (b + randf()) * oneoverN; // each sample must be random
      double theta = 2.0 * acos(sqrt(1.0 - x));
      double phi = 2.0 * M_PI * y;

      auto v = glm::normalize(glm::vec3{sin(theta)*cos(phi), sin(theta)*sin(phi), cos(theta)});
      //sample.w = glm::vec4(v.x, v.y, v.z, 1.f);
      sample.w[0] = v.x;
      sample.w[1] = v.y;
      sample.w[2] = v.z;
      for(int l=0; l< int(NUM_BANDS); ++l) {
        for(int m=-l; m<=l; ++m) {
          int index = l*(l+1)+m;
          sample.sh[index] = SHf(l,m,theta,phi);
          //std::cout << index << " " << l << " " << m << sample.sh[index] << "\n";
        }
      }
      temp_samples.push_back(sample);
    }
  }

  sh_samples = ds.storage.create_buffer(
    ds.ctx, drv::GPUMemoryT::Local, sizeof(SHSample) * temp_samples.size(), 
    vk::BufferUsageFlagBits::eTransferDst|vk::BufferUsageFlagBits::eStorageBuffer);

  ds.storage.buffer_memcpy(ds.ctx, sh_samples, 0, temp_samples.data(), sizeof(SHSample) * temp_samples.size());
}

void SHPass::init_shader_resources(DriverState &ds) {
  ds.pipelines.load_shader(ds.ctx, "integrate_sh_cs", "src/shaders/integrate_sh_comp.spv", vk::ShaderStageFlagBits::eCompute);

  drv::DescriptorSetLayoutBuilder res_builder{};
  res_builder
    .add_storage_buffer(0, vk::ShaderStageFlagBits::eCompute)
    .add_storage_buffer(1, vk::ShaderStageFlagBits::eCompute)
    .add_combined_sampler(2, vk::ShaderStageFlagBits::eCompute);

  resource_layout = ds.descriptors.create_layout(ds.ctx, res_builder.build(), 1);

  auto used_layouts = {ds.descriptors.get(resource_layout)};

  vk::PipelineLayoutCreateInfo p_layout_info{};
  p_layout_info.setSetLayouts(used_layouts);

  auto pipeline_layout = ds.ctx.get_device().createPipelineLayout(p_layout_info);
  pipeline = ds.pipelines.create_compute_pipeline(ds.ctx, "integrate_sh_cs", pipeline_layout);

  resources = ds.descriptors.allocate_set(ds.ctx, resource_layout);
}