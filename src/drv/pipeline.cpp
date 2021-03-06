#include "pipeline.hpp"

#include <fstream>
#include <iostream>

namespace drv {
  
  bool PipelineManager::load_shader(Context &ctx, const std::string &name, const std::string &path, vk::ShaderStageFlagBits stages, const std::string &proc) {
    if (shaders.find(name) != shaders.end()) {
      return false;
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
    u32 index = pipelines.size();
    bool push = true;
    if (free_index.size()) {
      index = free_index[free_index.size() - 1];
      free_index.pop_back();
      push = false;
    }

    if (push) {
      pipelines.push_back(info.desc);
    } else {
      pipelines[index] = info.desc;
    }
    
    auto &desc = pipelines[index];
    desc.handle = create_pipeline(ctx, desc);
    return index;
  }

  void PipelineManager::release(Context &ctx) {
    for (auto &d : shaders) {
      ctx.get_device().destroyShaderModule(d.second.mod);
    }

    for (auto &d : pipelines) {
      if (d.handle) ctx.get_device().destroyPipeline(d.handle);
      if (d.layout) ctx.get_device().destroyPipelineLayout(d.layout);
    }
  }

  void PipelineManager::reload_shaders(Context &ctx) {
    
    for (auto &desc : shaders) {
      desc.second.mod = load_shader(ctx, desc.second.path);
    }

    for (auto &p : pipelines) {
      if (p.handle && p.layout) {
        ctx.get_device().destroyPipeline(p.handle);
        p.handle = create_pipeline(ctx, p);
      }
    }

    for (auto &p : compute_pipelines) {
      if (p.handle && p.layout) {
        ctx.get_device().destroyPipeline(p.handle);
        p.handle = create_pipeline(ctx, p.module, p.layout);
      }
    }
  }

  void PipelineManager::free_pipeline(Context &ctx, PipelineID id) {
    ctx.get_device().destroyPipeline(pipelines[id].handle);
    ctx.get_device().destroyPipelineLayout(pipelines[id].layout);

    pipelines[id].handle = nullptr;
    pipelines[id].layout = nullptr;

    free_index.push_back(id);
  }

  vk::Pipeline PipelineManager::create_pipeline(Context &ctx, const std::string &shader_name, vk::PipelineLayout layout) {
    auto &shader = shaders.at(shader_name);

    vk::PipelineShaderStageCreateInfo stage {};
    stage.setStage(shader.stages);
    stage.setModule(shader.mod);
    stage.setPName(shader.proc.c_str());

    vk::ComputePipelineCreateInfo info {};
    info.setStage(stage);
    info.setLayout(layout);
    
    return ctx.get_device().createComputePipeline(nullptr, info);
  }

  ComputePipelineID PipelineManager::create_compute_pipeline(Context &ctx, const std::string &shader_name, vk::PipelineLayout layout) {
    ComputePipeline pipe {};
    pipe.module = shader_name;
    pipe.layout = layout;
    
    pipe.handle = create_pipeline(ctx, shader_name, layout);

    u32 size = compute_pipeline_free_index.size(); 

    if (size) {
      u32 ix = compute_pipeline_free_index[size - 1];
      compute_pipeline_free_index.pop_back();

      compute_pipelines[ix] = pipe;
      return ix;
    }

    compute_pipelines.push_back(pipe);
    return compute_pipelines.size() - 1;
  }

  void PipelineManager::free_pipeline(Context &ctx, ComputePipelineID id) {
    auto &desc = compute_pipelines[id];
    
    if (!desc.layout || !desc.handle) {
      throw std::runtime_error {"Attempt to double-free compute pipeline"};
    }

    ctx.get_device().destroyPipeline(desc.handle);
    ctx.get_device().destroyPipelineLayout(desc.layout);

    desc.handle = nullptr;
    desc.layout = nullptr;
    compute_pipeline_free_index.push_back(id);
  }
}