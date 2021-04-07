#ifndef FRAME_GLOBAL_HPP_INCLUDED
#define FRAME_GLOBAL_HPP_INCLUDED

#include "driverstate.hpp"
#include "postprocessing.hpp"
#include "scene.hpp"
#include "lightfield_probes.hpp"
#include "shpherical_harmonics.hpp"

#include <iostream>

struct FrameGlobal {
  void init(DriverState &ds) {
    scene.load("assets/Sponza/glTF/Sponza.gltf", "assets/Sponza/glTF/");
    scene.gen_buffers(ds);

    scene.add_light({0.f, 4.f, 0.f}, {10.f, 10.f, 10.f});
    scene.add_light({5.30641, 0.947165, -1.44263}, {0.f, 2.f, 0.f});
    scene.gen_shadows(ds);

    scene.gen_textures(ds);

    light_field.init(ds);
    light_field.render(ds, scene, glm::vec3{-10, 0.295498, -4}, glm::vec3{10, 2.50458, 4}, glm::uvec3{6, 3, 4});

    vk::SamplerCreateInfo smp {};
    smp
      .setMinFilter(vk::Filter::eLinear)
      .setMagFilter(vk::Filter::eLinear)
      .setMipmapMode(vk::SamplerMipmapMode::eLinear)
      .setMinLod(0.f)
      .setMaxLod(10.f);

    vk::SamplerCreateInfo smp2 {};
    smp2
      .setMinFilter(vk::Filter::eNearest)
      .setMagFilter(vk::Filter::eNearest)
      .setMipmapMode(vk::SamplerMipmapMode::eNearest)
      .setMinLod(0.f)
      .setMaxLod(10.f);
    
    default_sampler = ds.ctx.get_device().createSampler(smp);
    nearest_sampler = ds.ctx.get_device().createSampler(smp2);

    auto sh_pass = new SHPass{ds};
    sh_probes = sh_pass->integrate(ds, light_field.get_distance_array(), default_sampler);
    delete sh_pass;
  }

  void release(DriverState &ds) {
    light_field.release(ds);
    ds.ctx.get_device().destroySampler(default_sampler);
  }

  void update(float dt) {
    std::lock_guard<std::mutex> lock{frame_lock};
    camera.move(dt);
  }
  
  void handle_events(const SDL_Event &event) {
    std::lock_guard<std::mutex> lock{frame_lock};
    camera.process_event(event);

    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) {
      auto pos = camera.get_pos();
      std::cout << pos.x << " " << pos.y << " " << pos.z << "\n";
    }
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
  
  Scene &get_scene() { return scene; }

  LightField &get_light_field() { return light_field; }

  drv::BufferID &get_sh_probes() { return sh_probes; }
  vk::Sampler get_default_sampler() { return default_sampler; }
  vk::Sampler get_nearest_sampler() { return nearest_sampler; }
  
private:
  Scene scene;
  GBuffer gbuffer;
  LightField light_field;
  
  drv::BufferID sh_probes;
  vk::Sampler default_sampler;
  vk::Sampler nearest_sampler;

  std::mutex frame_lock;
  Camera camera;
  
  glm::mat4 camera_matrix;
  glm::vec3 camera_pos;
  const glm::mat4 projection = glm::perspective(glm::radians(60.f), 16.f/9.f, 0.01f, 100.f);
};

#endif