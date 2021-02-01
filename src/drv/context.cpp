#include "context.hpp"

#include <SDL2/SDL_vulkan.h>
#include <iostream>

namespace drv {
  static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                       VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                       const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                       void* pUserData)  {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
  }

  static std::vector<const char*> get_instance_extensions(SDL_Window *window) {
    u32 count;
    SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr);

    std::vector<const char*> res(count);
    SDL_Vulkan_GetInstanceExtensions(window, &count, res.data());
    return res;
  }

  void Context::init_instance() {
    vk::ApplicationInfo app_info {};
    app_info.setEngineVersion(VK_MAKE_VERSION(1, 0, 0));
    app_info.setApplicationVersion(VK_MAKE_VERSION(1, 0, 0));
    app_info.setPEngineName("NONAME");
    app_info.setPApplicationName("NOAPP");

    auto instance_ext = get_instance_extensions(window);
    instance_ext.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    auto layers = {"VK_LAYER_KHRONOS_validation"};

    vk::InstanceCreateInfo info {};
    info.setPApplicationInfo(&app_info);
    info.setPEnabledExtensionNames(instance_ext);
    info.setPEnabledLayerNames(layers);

    instance = vk::createInstance(info);
    

    auto devices = instance.enumeratePhysicalDevices();
    for (auto &dev : devices) {
      auto properties = dev.getProperties();
      std::cout << properties.deviceName << "\n";
    }

    
    auto create_messenger = (PFN_vkCreateDebugUtilsMessengerEXT)instance.getProcAddr("vkCreateDebugUtilsMessengerEXT");
    
    if (create_messenger) {
      VkDebugUtilsMessengerCreateInfoEXT createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
      createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
      createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
      createInfo.pfnUserCallback = debug_callback;
      createInfo.pUserData = nullptr;
      VkDebugUtilsMessengerEXT msg;
      auto res = create_messenger(instance, &createInfo, nullptr, &msg);
      if (res != VK_SUCCESS) {
        throw std::runtime_error {"Messenger create error"};
      }

      debug_messenger = msg;
    }

    VkSurfaceKHR surf;
    SDL_Vulkan_CreateSurface(window, instance, &surf);
    surface = surf;
  }

  void Context::init(SDL_Window *src) {
    window = src;
    
    init_instance();
    init_device();
    init_swapchain();
  }

  void Context::release() {
    if (swapchain) {
      device.destroySwapchainKHR(swapchain);
    }

    if (device) {
      device.destroy();
    }

    if (surface) {
      instance.destroySurfaceKHR(surface);
    }

    if (debug_messenger && instance) {
      auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT");
      if (func != nullptr) {
        func(instance, debug_messenger, nullptr);
      }
    }

    if (instance) {
      instance.destroy();
    }

  }

  static vk::PhysicalDevice pick_device(vk::Instance &instance) {
    auto devices = instance.enumeratePhysicalDevices();
    for (auto &dev : devices) {
      auto properties = dev.getProperties();
      if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
        std::cout << "Device " << properties.deviceName << "\n";
        return dev;
      }
    }

    throw std::runtime_error {"Physical device not found"};
    return {};
  }

  static void pick_queues(vk::PhysicalDevice &dev, QueueInfo out_queues[QUEUE_COUNT]) {
    auto queue_families = dev.getQueueFamilyProperties();
    
    bool queue_picked[QUEUE_COUNT] {};
    bool complete = true;
    u32 family_index = 0;

    for (auto family : queue_families) {
      u32 queue_index = 0;

      if ((family.queueFlags & GRAPHICS_QUEUE_FLAGS) == GRAPHICS_QUEUE_FLAGS) {
        out_queues[(u32)QueueT::Graphics].family = family_index;
        out_queues[(u32)QueueT::Graphics].index = queue_index;
        queue_picked[(u32)QueueT::Graphics] = true;
        family.queueCount--;
        queue_index++;
        std::cout << "GraphicsQueue family " << family_index << " index " <<  out_queues[(u32)QueueT::Graphics].index << "\n";
      }

      if ((family.queueFlags & TRANSFER_QUEUE_FLAGS) == TRANSFER_QUEUE_FLAGS && family.queueCount) {
        out_queues[(u32)QueueT::Transfer].family = family_index;
        out_queues[(u32)QueueT::Transfer].index = queue_index;
        queue_picked[(u32)QueueT::Transfer] = true;
        family.queueCount--;
        queue_index++;
        std::cout << "TransferQueue family " << family_index << " index " <<  out_queues[(u32)QueueT::Transfer].index << "\n";
      }

      family_index++;

      complete = true;

      for (u32 i = 0; i < QUEUE_COUNT; i++) {
        complete &= queue_picked[i];
      }
      
      if (complete) {
        break;
      }
    }
    
    if (!complete) {
      throw std::runtime_error {"Queue not found"};
    }
  }

  void Context::init_device() {
    physical_device = pick_device(instance);
    pick_queues(physical_device, queue_info);
    
    queue_family_indexes.push_back(queue_info[0].family);
    
    if (queue_info[1].family != queue_info[0].family) {
      queue_family_indexes.push_back(queue_info[1].family);
    }

    auto ok = physical_device.getSurfaceSupportKHR(queue_info[(u32)QueueT::Graphics].family, surface);
    if (ok != VK_TRUE) {
      throw std::runtime_error {"Device not support Surface!"};
    }

    std::vector<vk::DeviceQueueCreateInfo> queue_conf;

    auto queue_priorities = {1.f};

    if (queue_family_indexes.size() > 1) {
      queue_conf.resize(2);
      queue_conf[(u32)QueueT::Graphics]
      .setQueueCount(1)
      .setQueueFamilyIndex(queue_info[(u32)QueueT::Graphics].family)
      .setQueuePriorities(queue_priorities);

      queue_conf[(u32)QueueT::Transfer]
        .setQueueCount(1)
        .setQueueFamilyIndex(queue_info[(u32)QueueT::Transfer].family)
        .setQueuePriorities(queue_priorities);
    } else {
      auto queue_priorities = {1.f, 0.5f};
      vk::DeviceQueueCreateInfo info {};
      info
        .setQueueCount(QUEUE_COUNT)
        .setQueueFamilyIndex(queue_family_indexes[0])
        .setQueueCount(2)
        .setQueuePriorities(queue_priorities);
      queue_conf.push_back(info);
    }
    

    auto ext = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    vk::DeviceCreateInfo info {};
    info.setPEnabledExtensionNames(ext);
    info.setQueueCreateInfos(queue_conf);

    device = physical_device.createDevice(info);

    for (u32 i = 0; i < QUEUE_COUNT; i++) {
      queues[i] = device.getQueue(queue_info[i].family, queue_info[i].index);
    }
  }

  void Context::init_swapchain() {
    vk::SwapchainCreateInfoKHR info {};
  
    auto formats = physical_device.getSurfaceFormatsKHR(surface);
    auto modes = physical_device.getSurfacePresentModesKHR(surface);
    auto cap = physical_device.getSurfaceCapabilitiesKHR(surface);


    info.setPQueueFamilyIndices(&queue_family_indexes[(u32)QueueT::Graphics]);
    info.setQueueFamilyIndexCount(1);
    info.setImageFormat(formats.at(0).format);
    info.setImageColorSpace(vk::ColorSpaceKHR::eSrgbNonlinear);

    for (auto fmt : formats) {
      if (fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear && fmt.format == vk::Format::eB8G8R8A8Srgb) {
        info.setImageFormat(fmt.format);
        info.setImageColorSpace(fmt.colorSpace);
      }
    }

    swapchain_fmt = info.imageFormat;
    swapchain_colorspace = info.imageColorSpace;

    info.setPresentMode(vk::PresentModeKHR::eFifo);

    for (auto mode : modes) {
      if (mode == vk::PresentModeKHR::eMailbox) {
        info.setPresentMode(mode);
      }
    }

    info.setPreTransform(cap.currentTransform);
    info.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);
    info.setImageSharingMode(vk::SharingMode::eExclusive);
    info.setImageExtent(cap.currentExtent);
    swapchain_ext = info.imageExtent;
    
    info.setOldSwapchain({});
    info.setClipped(VK_FALSE);

    info.setImageArrayLayers(1);
    
    u32 min_img_count = cap.minImageCount + 1;
    min_img_count = max(min(min_img_count, cap.maxImageCount), cap.minImageCount);

    info.setMinImageCount(min_img_count);
    info.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
    info.setSurface(surface);
    swapchain = device.createSwapchainKHR(info);
    swapchain_images = device.getSwapchainImagesKHR(swapchain);
  }

  u32 Context::queue_index(QueueT qtype) const {
    return queue_info[(u32)qtype].family;
  }

}