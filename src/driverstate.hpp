#ifndef DRIVER_STATE_HPP_INCLUDED
#define DRIVER_STATE_HPP_INCLUDED

#include "drv/context.hpp"
#include "drv/descriptors.hpp"
#include "drv/pipeline.hpp"
#include "drv/resources.hpp"
#include "drv/draw_context.hpp"

#include "camera.hpp"

#include <mutex>

struct DriverState {
  drv::Context ctx;
  drv::ResourceStorage storage;
  drv::DescriptorStorage descriptors;
  drv::PipelineManager pipelines;
  drv::DrawContextPool submit_pool;
  vk::RenderPass main_renderpass;
};


struct FrameGlobal {

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

  glm::mat4 get_camera_matrix() { 
    std::lock_guard<std::mutex> lock{frame_lock};
    return camera.get_view_mat(); 
  }

  glm::mat4 get_projection_matrix() const {
    return projection;
  }

private:
  std::mutex frame_lock;
  Camera camera;
  const glm::mat4 projection = glm::perspective(glm::radians(60.f), 4.f/3.f, 0.01f, 15.f);
};

#endif