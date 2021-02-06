#include "pipeline.hpp"

#include <fstream>
#include <iostream>

namespace drv {
  
  bool PipelineManager::load_shader(Context &ctx, const std::string &name, const std::string &path, vk::ShaderStageFlagBits stages, const std::string &proc) {
    if (shaders.find(name) != shaders.end()) {
      throw std::runtime_error {" shader redefenition"};
    }
  
    ShaderDesc desc;
    desc.mod = load_shader(ctx, path);
    desc.path = path;
    desc.proc = proc;
    desc.stages = stages;

    shaders.insert({name, desc});
    return true;
  }

  vk::ShaderModule PipelineManager::load_shader(Context &ctx, const std::string &path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
      throw std::runtime_error("failed to open file!");
    }
    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    vk::ShaderModuleCreateInfo info {};

    info.setPCode((u32*)buffer.data());
    info.setCodeSize(buffer.size());

    return ctx.get_device().createShaderModule(info);
  }

  vk::Pipeline PipelineManager::create_pipeline(Context &ctx, PipelineManager::PipelineDesc &desc) {
    bool has_vertex_input = !desc.input.bindings.empty() && !desc.input.attributes.empty();
    
    vk::PipelineVertexInputStateCreateInfo input {};
    input
      .setVertexAttributeDescriptions(desc.input.attributes)
      .setVertexBindingDescriptions(desc.input.bindings);

    auto blend = desc.blend_state.build();

    vk::PipelineViewportStateCreateInfo viewport {};
    viewport
      .setViewports(desc.viewports)
      .setScissors(desc.scissors);

    auto dyn_state = desc.build_dyn();

    std::vector<vk::PipelineShaderStageCreateInfo> stages;
    
    for (const auto& sname : desc.modules) {
      auto &shader = shaders.at(sname);
      vk::PipelineShaderStageCreateInfo next {};
      next
        .setModule(shader.mod)
        .setStage(shader.stages)
        .setPName(shader.proc.c_str());

      stages.push_back(next);
    }

    vk::PipelineMultisampleStateCreateInfo ms {};

    vk::GraphicsPipelineCreateInfo p {};
    p
      .setLayout(desc.layout)
      .setRenderPass(desc.renderpass)
      .setSubpass(desc.subpass)
      .setStages(stages)
      .setPVertexInputState(&input)
      .setPInputAssemblyState(&desc.assembly)
      .setPRasterizationState(&desc.raster)
      .setPDepthStencilState(&desc.depth_state)
      .setPViewportState(&viewport)
      .setPColorBlendState(&blend)
      .setPDynamicState(&dyn_state)
      .setPMultisampleState(&ms);
    
    auto h = ctx.get_device().createGraphicsPipeline(nullptr, p);
    return h.value;
  }

  PipelineID PipelineManager::create_pipeline(Context &ctx, const PipelineDescBuilder &info) {
    pipelines.push_back(info.desc);
    u32 index = pipelines.size() - 1;
    auto &desc = pipelines[index];
    desc.handle = create_pipeline(ctx, desc);
    return index;
  }

  void PipelineManager::release(Context &ctx) {
    for (auto &d : shaders) {
      ctx.get_device().destroyShaderModule(d.second.mod);
    }

    for (auto &d : pipelines) {
      ctx.get_device().destroyPipeline(d.handle);
    }
  }

  void PipelineManager::reload_shaders(Context &ctx) {
    release(ctx);
    
    for (auto &desc : shaders) {
      desc.second.mod = load_shader(ctx, desc.second.path);
    }

    for (auto &p : pipelines) {
      p.handle = create_pipeline(ctx, p);
    }
  }
}