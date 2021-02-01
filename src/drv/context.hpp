#ifndef CONTEXT_HPP_INCLUDED
#define CONTEXT_HPP_INCLUDED

#include "common.hpp"

#include <SDL2/SDL.h>

namespace drv {
  const u32 QUEUE_COUNT = 2;
  
  const vk::QueueFlags GRAPHICS_QUEUE_FLAGS = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer;
  const vk::QueueFlags TRANSFER_QUEUE_FLAGS = vk::QueueFlagBits::eTransfer|vk::QueueFlagBits::eGraphics;

  enum class QueueT : u32 {
    Graphics = 0,
    Transfer = 1,
    Count
  };

  struct QueueInfo {
    u32 family;
    u32 index;
  };

  struct Context {
    Context() {}
    void init(SDL_Window *w);
    void release();

    std::vector<vk::Image> &get_swapchain_images() { return swapchain_images; }
    vk::Device &get_device() { return device; }
    vk::SwapchainKHR &get_swapchain() { return swapchain; }
    vk::PhysicalDevice &get_physical_device() { return physical_device; }

    vk::Format get_swapchain_fmt() const { return swapchain_fmt; }
    vk::Extent2D get_swapchain_extent() const { return swapchain_ext; }

    u32 queue_index(QueueT qtype) const;
    const u32* get_queue_indexes() const { return queue_family_indexes.data(); }
    const u32 queue_family_count() const { return queue_family_indexes.size(); }
    
    vk::Queue &get_queue(QueueT qtype) { return queues[(u32)qtype]; }

    Context(Context&) = delete;
    const Context& operator=(const Context&) = delete;
  private:
    void init_instance();
    void init_device();
    void init_swapchain();

    vk::Instance instance;
    vk::DebugUtilsMessengerEXT debug_messenger;
    SDL_Window *window = nullptr;

    vk::Device device;
    vk::PhysicalDevice physical_device;
    vk::Queue queues[QUEUE_COUNT];
    std::vector<u32> queue_family_indexes;
    QueueInfo queue_info[QUEUE_COUNT];

    vk::SurfaceKHR surface;
    vk::SwapchainKHR swapchain;
    vk::Format swapchain_fmt;
    vk::ColorSpaceKHR swapchain_colorspace;
    vk::Extent2D swapchain_ext;
    std::vector<vk::Image> swapchain_images;
    //std::vector<vk::ImageView> surface_views;
  };
} // namespace drv


#endif