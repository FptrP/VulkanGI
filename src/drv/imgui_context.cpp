#include "imgui_context.hpp"

#include "lib/vkimgui/imgui.h"
#include "lib/vkimgui/imgui_impl_sdl.h"
#include "lib/vkimgui/imgui_impl_vulkan.h"

#include <iostream>

namespace drv {

  static void check_vk_result(VkResult err)
  {
    if (err == 0)
        return;
    std::cerr <<  "[vulkan] Error: VkResult = " <<  u32(err) << "\n";
    if (err < 0)
        throw std::runtime_error {"Vulkan error"};
  }

  void ImguiContext::init(Context &ctx, vk::RenderPass &renderpass, u32 subpass) {

    {
      vk::DescriptorPoolSize pool_sizes[] {
        { vk::DescriptorType::eSampler, 100 },
        { vk::DescriptorType::eCombinedImageSampler, 100 },
        { vk::DescriptorType::eSampledImage, 100 },
        { vk::DescriptorType::eStorageImage, 100 },
        { vk::DescriptorType::eUniformTexelBuffer, 100 },
        { vk::DescriptorType::eStorageTexelBuffer, 100 },
        { vk::DescriptorType::eUniformBuffer, 100 },
        { vk::DescriptorType::eStorageImage, 100 },
        { vk::DescriptorType::eUniformBufferDynamic, 100 },
        { vk::DescriptorType::eStorageBufferDynamic, 100 },
        { vk::DescriptorType::eInputAttachment, 100 }
      };

      vk::DescriptorPoolCreateInfo info {};
      info.setMaxSets(1100);
      info.setPPoolSizes(pool_sizes);
      info.setPoolSizeCount(11);
      info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);

      pool = ctx.get_device().createDescriptorPool(info);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForVulkan(ctx.get_window());
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = static_cast<VkInstance>(ctx.get_instance());
    init_info.PhysicalDevice = static_cast<VkPhysicalDevice>(ctx.get_physical_device());
    init_info.Device = static_cast<VkDevice>(ctx.get_device());
    init_info.QueueFamily = ctx.queue_index(QueueT::Graphics);
    init_info.Queue = static_cast<VkQueue>(ctx.get_queue(QueueT::Graphics));
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = static_cast<VkDescriptorPool>(pool);
    init_info.Allocator = nullptr;
    init_info.MinImageCount = ctx.get_swapchain_images().size();
    init_info.ImageCount = ctx.get_swapchain_images().size();
    init_info.CheckVkResultFn = check_vk_result;
    init_info.Subpass = subpass;
    ImGui_ImplVulkan_Init(&init_info, static_cast<VkRenderPass>(renderpass));

    window = ctx.get_window();

  }

  void ImguiContext::new_frame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();
  }

  void ImguiContext::render(vk::CommandBuffer &cmd) {
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();

    ImGui_ImplVulkan_RenderDrawData(draw_data, static_cast<VkCommandBuffer>(cmd));
  }

  void ImguiContext::release(Context &ctx) {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
  }

  void ImguiContext::process_event(const SDL_Event &event) {
    ImGui_ImplSDL2_ProcessEvent(&event);
  }

  void ImguiContext::create_fonts(Context &ctx, DrawContextPool &ctx_pool) {
    auto cmd = ctx_pool.start_cmd(ctx);
    
    vk::CommandBufferBeginInfo info {};
    cmd.begin(info);
    ImGui_ImplVulkan_CreateFontsTexture(cmd);
    cmd.end();
    auto fence = ctx_pool.submit_cmd(ctx, cmd);

    ctx.get_device().waitForFences({fence}, VK_TRUE, ~0ul);
    ctx.get_device().destroyFence(fence);
  }

}