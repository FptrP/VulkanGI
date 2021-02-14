#ifndef FRAME_GLOBAL_HPP_INCLUDED
#define FRAME_GLOBAL_HPP_INCLUDED

#include "driverstate.hpp"
#include "postprocessing.hpp"
#include "scene.hpp"
#include "lightfield_probes.hpp"

struct FrameGlobal {
  void init(DriverState &ds) {
    scene.load("assets/Sponza/glTF/Sponza.gltf", "assets/Sponza/glTF/");
    scene.gen_buffers(ds);

    scene.add_light({0.f, 4.f, 0.f}, {1.f, 1.f, 1.f});
    scene.gen_shadows(ds);

    scene.gen_textures(ds);

    light_field.init(ds);
    light_field.render(ds, scene, glm::vec3{0.f, 5.f, 0.f}, glm::vec3{2.f, 2.f, 2.f}, glm::uvec3{3, 3, 3});
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