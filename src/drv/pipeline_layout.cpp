#include "pipeline.hpp"


namespace drv {
  /*
  using Self = PipelineLayoutBuilder&;

  Self& PipelineLayoutBuilder::add_ubo(u32 binding, vk::ShaderStageFlags stage) {
    vk::DescriptorSetLayoutBinding elem {};

    elem.setBinding(binding);
    elem.setDescriptorCount(1);
    elem.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    elem.setStageFlags(stage);

    bindings.push_back(elem);

    return *this;
  }

  vk::PipelineLayoutCreateInfo PipelineLayoutBuilder::build() {
    vk::PipelineLayoutCreateInfo info {};

    

    return {};
  }*/

  DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::add_ubo(u32 binding, vk::ShaderStageFlags stages) {
    vk::DescriptorSetLayoutBinding elem {};

    elem.setBinding(binding);
    elem.setDescriptorCount(1);
    elem.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    elem.setStageFlags(stages);

    bindings.push_back(elem);

    return *this;
  }

  DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::add_combined_sampler(u32 binding, vk::ShaderStageFlags stages) {
    vk::DescriptorSetLayoutBinding elem {};
    elem.setBinding(binding);
    elem.setDescriptorCount(1);
    elem.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
    elem.setStageFlags(stages);
    
    bindings.push_back(elem);
    return *this;
  }

  DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::add_storage_buffer(u32 binding, vk::ShaderStageFlags stages) {
    vk::DescriptorSetLayoutBinding elem {};
    elem.setBinding(binding);
    elem.setDescriptorCount(1);
    elem.setDescriptorType(vk::DescriptorType::eStorageBuffer);
    elem.setStageFlags(stages);
    
    bindings.push_back(elem);
    return *this;
  }

  DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::add_sampler(u32 binding, vk::ShaderStageFlags stages) {
    vk::DescriptorSetLayoutBinding elem {};
    elem.setBinding(binding);
    elem.setDescriptorCount(1);
    elem.setDescriptorType(vk::DescriptorType::eSampler);
    elem.setStageFlags(stages);
    bindings.push_back(elem);
    return *this;
  }

  DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::add_array_of_tex(u32 binding, u32 count, vk::ShaderStageFlags stages) {
    vk::DescriptorSetLayoutBinding elem {};
    elem.setBinding(binding);
    elem.setDescriptorCount(count);
    elem.setDescriptorType(vk::DescriptorType::eSampledImage);
    elem.setStageFlags(stages);
    bindings.push_back(elem);
    return *this;
  }

  vk::DescriptorSetLayoutCreateInfo DescriptorSetLayoutBuilder::build() {
    vk::DescriptorSetLayoutCreateInfo info {};
    info.setBindings(bindings);
    return info;
  }

}