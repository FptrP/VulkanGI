#ifndef FRAME_GLOBAL_HPP_INCLUDED
#define FRAME_GLOBAL_HPP_INCLUDED

#include "driverstate.hpp"
#include "postprocessing.hpp"
#include "scene.hpp"
#include "lightfield_probes.hpp"

#include <iostream>

struct FrameGlobal {
  void init(DriverState &ds) {
    scene.load("assets/Sponza/glTF/Sponza.gltf", "assets/Sponza/glTF/");
    scene.gen_buffers(ds);

    scene.add_light({0.f, 4.f, 0.f}, {1.f, 1.f, 1.f});
    scene.gen_shadows(ds);

    scene.gen_textures(ds);

    light_field.init(ds);
    light_field.render(ds, scene, glm::vec3{-2.27381, 0.295498, -1.23232}, glm::vec3{2.16548, 4.50458, 0.63653}, glm::uvec3{3, 3, 3});
  }

  void release(DriverState &ds) {
    light_field.release(ds);
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

private:
  Scene scene;
  GBuffer gbuffer;
  LightField light_field;

  std::mutex frame_lock;
  Camera camera;
  
  glm::mat4 camera_matrix;
  glm::vec3 camera_pos;
  const glm::mat4 projection = glm::perspective(glm::radians(60.f), 4.f/3.f, 0.01f, 15.f);
};

#endif