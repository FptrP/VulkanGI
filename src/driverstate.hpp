#ifndef DRIVER_STATE_HPP_INCLUDED
#define DRIVER_STATE_HPP_INCLUDED

#include "drv/context.hpp"
#include "drv/descriptors.hpp"
#include "drv/pipeline.hpp"
#include "drv/resources.hpp"
#include "drv/draw_context.hpp"

#include "camera.hpp"

#include <glm/glm.hpp>
#include <mutex>

struct DriverState {
  drv::Context ctx;
  drv::ResourceStorage storage;
  drv::DescriptorStorage descriptors;
  drv::PipelineManager pipelines;
  drv::DrawContextPool submit_pool;
  vk::RenderPass main_renderpass;
};

struct GBuffer {
  vk::Sampler sampler; 
  std::vector<drv::ImageViewID> images; //albedo, normal, world_pos, depth

  void init(DriverState &ds) {
    auto screen = ds.ctx.get_swapchain_extent();
    auto albedo_img = ds.storage.create_rt(
      ds.ctx, 
      screen.width, 
      screen.height, 
      vk::Format::eR8G8B8A8Srgb, 
      vk::ImageUsageFlagBits::eInputAttachment|vk::ImageUsageFlagBits::eColorAttachment);

    auto normal_img = ds.storage.create_rt(
      ds.ctx, 
      screen.width, 
      screen.height, 
      vk::Format::eR16G16B16A16Sfloat, 
      vk::ImageUsageFlagBits::eInputAttachment|vk::ImageUsageFlagBits::eColorAttachment);
    
    auto worldpos_img = ds.storage.create_rt(
      ds.ctx, 
      screen.width, 
      screen.height, 
      vk::Format::eR16G16B16A16Sfloat, 
      vk::ImageUsageFlagBits::eInputAttachment|vk::ImageUsageFlagBits::eColorAttachment);
    
    auto depth_img = ds.storage.create_rt(
      ds.ctx, 
      screen.width, 
      screen.height, 
      vk::Format::eD24UnormS8Uint, 
      vk::ImageUsageFlagBits::eInputAttachment|vk::ImageUsageFlagBits::eDepthStencilAttachment);

    auto albedo_view = ds.storage.create_rt_view(ds.ctx, albedo_img, vk::ImageAspectFlagBits::eColor);
    auto normal_view = ds.storage.create_rt_view(ds.ctx, normal_img, vk::ImageAspectFlagBits::eColor);
    auto worldpos_view = ds.storage.create_rt_view(ds.ctx, worldpos_img, vk::ImageAspectFlagBits::eColor);
    auto depth_view = ds.storage.create_rt_view(ds.ctx, depth_img, vk::ImageAspectFlagBits::eDepth);

    images.push_back(albedo_view);
    images.push_back(normal_view);
    images.push_back(worldpos_view);
    images.push_back(depth_view);

    vk::SamplerCreateInfo smp {};
    smp
      .setMinFilter(vk::Filter::eLinear)
      .setMagFilter(vk::Filter::eLinear)
      .setMipmapMode(vk::SamplerMipmapMode::eLinear)
      .setMinLod(0.f)
      .setMaxLod(10.f);
    
    sampler = ds.ctx.get_device().createSampler(smp);
  }

  void release(DriverState &ds) {
    ds.ctx.get_device().destroySampler(sampler);
  }
};

struct FrameGlobal {
  void init(DriverState &ds) {
    gbuffer.init(ds);
  }

  void release(DriverState &ds) {
    gbuffer.release(ds);
  }

  void update(float dt) {
    std::lock_guard<std::mutex> lock{frame_lock};
    camera.move(dt);
  }
  
  void handle_events(const SDL_Event &event) {
    std::lock_guard<std::mutex> lock{frame_lock};
    camera.process_event(event);
  }

  void lock() {
    frame_lock.lock();
  }

  void unlock() {
    frame_lock.unlock();
  }

  glm::mat4 get_camera_matrix(bool update = true) {
    if (update) {
      std::lock_guard<std::mutex> lock{frame_lock};
      camera_matrix = camera.get_view_mat();
      camera_pos = camera.get_pos(); 
    } 
    return camera_matrix;
  }

  glm::vec3 get_camera_pos() const {
    return camera_pos;
  }

  glm::mat4 get_projection_matrix() const {
    return projection;
  }

  GBuffer &get_gbuffer() { return gbuffer; }
  const GBuffer &get_gbuffer() const { return gbuffer; }
  
  void set_cubemap(const drv::ImageViewID &cm) { cubemap = cm; }

  drv::ImageViewID &get_cubemap() { return cubemap; }

private:
  GBuffer gbuffer;
  drv::ImageViewID cubemap;

  std::mutex frame_lock;
  Camera camera;
  
  glm::mat4 camera_matrix;
  glm::vec3 camera_pos;
  const glm::mat4 projection = glm::perspective(glm::radians(60.f), 4.f/3.f, 0.01f, 15.f);
};

#endif