#ifndef PIPELINE_HPP_INCLUDED
#define PIPELINE_HPP_INCLUDED

#include "common.hpp"
#include "context.hpp"

#include <string>
#include <map>

namespace drv {

  struct DescriptorSetLayoutBuilder {
    DescriptorSetLayoutBuilder() {}
    using Self = DescriptorSetLayoutBuilder&;

    Self add_ubo(u32 binding, vk::ShaderStageFlags stages);
    Self add_combined_sampler(u32 binding, vk::ShaderStageFlags stages);
    Self add_storage_buffer(u32 binding, vk::ShaderStageFlags stages);
    Self add_sampler(u32 binding, vk::ShaderStageFlags stages);
    Self add_array_of_tex(u32 binding, u32 count, vk::ShaderStageFlags stages);
    Self add_input_attachment(u32 binding, vk::ShaderStageFlags stages);
    //Self add_cubemap_sampler(u32 binding, vk::ShaderStageFlags stages);

    vk::DescriptorSetLayoutCreateInfo build();
  
  private:
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
  };

  struct PipelineID {
    PipelineID() {}
    PipelineID(u32 i) : index {i} {};
    operator u32() const { return index; }
  private:
    u32 index = ~0u;
  };

  struct PipelineDescBuilder;

  struct PipelineManager {
    bool load_shader(Context &ctx, const std::string &name, const std::string &path, vk::ShaderStageFlagBits stages, const std::string &proc = "main");    
    void reload_shaders(Context &ctx);

    PipelineID create_pipeline(Context &ctx, const PipelineDescBuilder &info);
    void free_pipeline(Context &ctx, PipelineID id);

    vk::Pipeline &get(PipelineID id) { return pipelines[id].handle; }
    vk::PipelineLayout &get_layout(PipelineID id) { return pipelines[id].layout; }

    const vk::Pipeline &get(PipelineID id) const { return pipelines[id].handle; }
    const vk::PipelineLayout &get_layout(PipelineID id) const { return pipelines[id].layout; }

    void release(Context &ctx);

  private:
    struct PipelineDesc;

    vk::ShaderModule load_shader(Context &ctx, const std::string &path);
    vk::Pipeline create_pipeline(Context &ctx, PipelineDesc &desc);

    struct ShaderDesc {
      std::string path;
      std::string proc;
      vk::ShaderStageFlagBits stages;
      vk::ShaderModule mod;
    };

    struct VertexDesc {
      std::vector<vk::VertexInputBindingDescription> bindings;
      std::vector<vk::VertexInputAttributeDescription> attributes;

      vk::PipelineVertexInputStateCreateInfo build() const {
        vk::PipelineVertexInputStateCreateInfo info {};
        info
          .setVertexAttributeDescriptions(attributes)
          .setVertexBindingDescriptions(bindings);

        return info;
      }
    };

    struct BlendStateDesc {
      vk::PipelineColorBlendStateCreateInfo info{};
      std::vector<vk::PipelineColorBlendAttachmentState> attachmens;

      vk::PipelineColorBlendStateCreateInfo build() {
        info.setAttachments(attachmens);
        info.setLogicOp(vk::LogicOp::eCopy);
        return info;
      }
    };

    struct PipelineDesc {
      std::vector<std::string> modules;
      
      VertexDesc input{};
      vk::PipelineInputAssemblyStateCreateInfo assembly {}; 
      
      std::vector<vk::Viewport> viewports;
      std::vector<vk::Rect2D> scissors;

      vk::PipelineRasterizationStateCreateInfo raster {};

      vk::PipelineDepthStencilStateCreateInfo depth_state {};
      
      BlendStateDesc blend_state {};
      
      std::vector<vk::DynamicState> dynamic_states;
      
      vk::PipelineLayout layout;
      vk::RenderPass renderpass;
      u32 subpass;
      
      vk::Pipeline handle;

      vk::PipelineViewportStateCreateInfo build_vp() {
        vk::PipelineViewportStateCreateInfo info {};
        info
          .setViewports(viewports)
          .setScissors(scissors);
        return info;
      }

      vk::PipelineDynamicStateCreateInfo build_dyn() {
        vk::PipelineDynamicStateCreateInfo info {};
        info.setDynamicStates(dynamic_states);
        return info;
      }
    };

    std::map<std::string, ShaderDesc> shaders;
    std::vector<PipelineDesc> pipelines;
    std::vector<u32> free_index;
    friend PipelineDescBuilder;
  };


  struct PipelineDescBuilder {
    
    PipelineDescBuilder() {
      desc.raster.setLineWidth(1.f);
      desc.input.attributes.clear();
      desc.input.bindings.clear();
      //desc.raster.setRasterizerDiscardEnable(VK_TRUE);
    }

    PipelineDescBuilder& attach_to_renderpass(const vk::RenderPass &rp, u32 subpass) {
      desc.subpass = subpass;
      desc.renderpass = rp;
      return *this;
    }
    PipelineDescBuilder& set_layout(const vk::PipelineLayout &layout) {
      desc.layout = layout;
      return *this;
    }
    
    PipelineDescBuilder& add_dynamic_state(vk::DynamicState state) {
      desc.dynamic_states.push_back(state);
      return *this;
    }

    //InputAssembly
    PipelineDescBuilder& set_vertex_assembly(vk::PrimitiveTopology pt, bool restart) {
      desc.assembly
        .setPrimitiveRestartEnable(restart? VK_TRUE : VK_FALSE)
        .setTopology(pt);
      return *this;
    }
  
    //Rasterization
    PipelineDescBuilder& set_line_width(float w) {
      desc.raster.setLineWidth(w);
      return *this;
    }
    PipelineDescBuilder& set_cull_mode(vk::CullModeFlags flags) {
      desc.raster.setCullMode(flags);
      return *this;
    }
    PipelineDescBuilder& set_front_face(vk::FrontFace face) {
      desc.raster.setFrontFace(face);
      return *this;
    }
    PipelineDescBuilder& set_polygon_mode(vk::PolygonMode mode) {
      desc.raster.setPolygonMode(mode);
      return *this;
    }

    //Depth-stencil
    PipelineDescBuilder& set_depth_write(bool enable) {
      desc.depth_state.setDepthWriteEnable(enable? VK_TRUE : VK_FALSE);
      return *this;
    }
    PipelineDescBuilder& set_depth_test(bool enable) {
      desc.depth_state.setDepthTestEnable(enable? VK_TRUE : VK_FALSE);
      return *this;
    }
    PipelineDescBuilder& set_depth_func(vk::CompareOp op) {
      desc.depth_state.setDepthCompareOp(op);
      return *this;
    }
    //PipelineDescBuilder& set_depth_bounds(bool enable);
    //PipelineDescBuilder& set_depth_bounds(float min, float max);

    //ViewportScissors
    PipelineDescBuilder& add_viewport(float x, float y, float w, float h, float min_d, float max_d) {
      return add_viewport(vk::Viewport{x, y, w, h, min_d, max_d});
    }
    PipelineDescBuilder& add_viewport(const vk::Viewport &vp) {
      desc.viewports.push_back(vp);
      return *this;
    }
    PipelineDescBuilder& add_scissors(i32 x, i32 y, u32 w, u32 h) {
      return add_scissors(vk::Rect2D {{x, y}, {w, h}});
    }
    PipelineDescBuilder& add_scissors(const vk::Rect2D &area) {
      desc.scissors.push_back(area);
      return *this;
    }

    //Shaders
    PipelineDescBuilder& add_shader(const std::string &name) {
      desc.modules.push_back(name);
      return *this;
    }

    //VertexInput 
    PipelineDescBuilder& add_attribute(u32 loc, u32 binding, vk::Format fmt, u32 offset) {
      vk::VertexInputAttributeDescription attr {};
      attr.format = fmt;
      attr.location = loc;
      attr.offset = offset;
      attr.binding = binding;
      desc.input.attributes.push_back(attr);
      return *this;
    }

    PipelineDescBuilder& add_binding(u32 binding, u32 stride, vk::VertexInputRate rate) {
      vk::VertexInputBindingDescription bd {};
      bd.binding = binding;
      bd.stride = stride;
      bd.inputRate = rate;
      desc.input.bindings.push_back(bd);
      return *this;
    }

    //blending
    PipelineDescBuilder& set_blend_constants(float r, float g, float b, float a) {
      desc.blend_state.info.blendConstants[0] = r;
      desc.blend_state.info.blendConstants[1] = g;
      desc.blend_state.info.blendConstants[2] = b;
      desc.blend_state.info.blendConstants[3] = a;
      return *this;
    }
    PipelineDescBuilder& add_blend_attachment() {
      vk::PipelineColorBlendAttachmentState state {};
      state.blendEnable = VK_FALSE;
      state.setColorWriteMask(vk::ColorComponentFlagBits::eB|vk::ColorComponentFlagBits::eA|vk::ColorComponentFlagBits::eG|vk::ColorComponentFlagBits::eR);
      desc.blend_state.attachmens.push_back(state);
      return *this;
    }
    PipelineDescBuilder& set_blend_logic_op(bool enable) {
      desc.blend_state.info.logicOpEnable = enable? VK_TRUE:VK_FALSE;
      return *this;
    }


  private:
    PipelineManager::PipelineDesc desc;

    friend PipelineManager;
  };

}


#endif