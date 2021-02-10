#ifndef CMD_UTILS_HPP_INCLUDED
#define CMD_UTILS_HPP_INCLUDED

#include "context.hpp"
#include "resources.hpp"

namespace drv {

  struct ImageBarrier {
    ImageBarrier(const vk::Image &img, vk::ImageAspectFlags flags);
    ImageBarrier(const ImageID &img, vk::ImageAspectFlags flags);

    ImageBarrier &access_msk(vk::AccessFlags src, vk::AccessFlags dst) {
      barrier.setSrcAccessMask(src);
      barrier.setDstAccessMask(dst);
      return *this;
    }
    ImageBarrier &change_queue(u32 src, u32 dst) {
      barrier.setSrcQueueFamilyIndex(src);
      barrier.setDstQueueFamilyIndex(dst);
      return *this;
    }

    ImageBarrier &change_layout(vk::ImageLayout src, vk::ImageLayout dst) {
      barrier.setOldLayout(src);
      barrier.setNewLayout(dst);
      return *this;
    }

    ImageBarrier &set_range(u32 base_mip = 0 , u32 mip_count = 1, u32 base_layer = 0, u32 layer_count = 1) {
      range.setBaseMipLevel(base_mip);
      range.setBaseArrayLayer(base_layer);
      range.setLevelCount(mip_count);
      range.setLayerCount(layer_count);
      return *this;
    }

    ImageBarrier &set_mip_range(u32 base_mip, u32 mip_count) {
      range.setBaseMipLevel(base_mip);
      range.setLevelCount(mip_count);
      return *this;
    }

    ImageBarrier &set_base_mip(u32 base_mip) {
      range.setBaseMipLevel(base_mip);
      return *this;
    }


    void write(vk::CommandBuffer &cmd, vk::PipelineStageFlags src, vk::PipelineStageFlags dst);

  private:
    vk::ImageMemoryBarrier barrier {};
    vk::ImageSubresourceRange range {};
  };

  struct BlitImage {
    BlitImage(ImageID &src, ImageID &dst);
    BlitImage(vk::Image s, vk::Image d) : src{s}, dst{d} {}

    BlitImage &src_subresource(vk::ImageAspectFlags flags, u32 mip = 0, u32 base_layer = 0, u32 layer_count = 1) {
      src_range
        .setAspectMask(flags)
        .setBaseArrayLayer(base_layer)
        .setMipLevel(mip)
        .setLayerCount(layer_count);
      return *this;
    }

    BlitImage &dst_subresource(vk::ImageAspectFlags flags, u32 mip = 0, u32 base_layer = 0, u32 layer_count = 1) {
      dst_range
        .setAspectMask(flags)
        .setBaseArrayLayer(base_layer)
        .setLayerCount(layer_count)
        .setMipLevel(mip);
      return *this;
    }
    
    BlitImage &src_offset(vk::Offset3D from, vk::Offset3D to) {
      src_offs[0] = from; src_offs[1] = to;
      return *this;
    }

    BlitImage &dst_offset(vk::Offset3D from, vk::Offset3D to) {
      dst_offs[0] = from; dst_offs[1] = to;
      return *this;
    }

    void write(vk::CommandBuffer &cmd);

  private:
    vk::ImageSubresourceLayers src_range {}, dst_range {};
    std::array<vk::Offset3D, 2u> src_offs {}, dst_offs {};
    vk::ImageBlit region {};
    vk::Image src, dst;
    vk::Filter filter = vk::Filter::eLinear;
  };

}

#endif