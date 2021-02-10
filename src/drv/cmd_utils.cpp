#include "cmd_utils.hpp"

namespace drv {
  
  ImageBarrier::ImageBarrier(const vk::Image &img, vk::ImageAspectFlags flags) {
    barrier.setImage(img);
    range
      .setAspectMask(flags)
      .setLayerCount(1)
      .setLevelCount(1);

    barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
  }

  ImageBarrier::ImageBarrier(const ImageID &img, vk::ImageAspectFlags flags) {
    barrier.setImage(img->api_image());
    barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);

    const auto &info = img->get_info();
    
    range.setAspectMask(flags);
    range.setLayerCount(info.arrayLayers);
    range.setLevelCount(info.mipLevels);

  }

  void ImageBarrier::write(vk::CommandBuffer &cmd, vk::PipelineStageFlags src, vk::PipelineStageFlags dst) {
    barrier.setSubresourceRange(range);
    auto img_barriers = {barrier};
    cmd.pipelineBarrier(src, dst, vk::DependencyFlags(0), {}, {}, img_barriers);
  }

  BlitImage::BlitImage(ImageID &s, ImageID &d) {
    src = s->api_image();
    auto &src_info = s->get_info();
    dst = d->api_image();
    auto &dst_info = d->get_info();

    src_offs[0] = vk::Offset3D{0, 0, 0};
    src_offs[1] = vk::Offset3D{(i32)src_info.extent.width, (i32)src_info.extent.height, 0};
    dst_offs[0] = vk::Offset3D{0, 0, 0};
    dst_offs[1] = vk::Offset3D{(i32)dst_info.extent.width, (i32)dst_info.extent.height, 0};
  }

  void BlitImage::write(vk::CommandBuffer &cmd) {
    region
      .setSrcSubresource(src_range)
      .setDstSubresource(dst_range)
      .setSrcOffsets(src_offs)
      .setDstOffsets(dst_offs);

    cmd.blitImage(src, vk::ImageLayout::eTransferSrcOptimal, dst, vk::ImageLayout::eTransferDstOptimal, {region}, filter);  
  }

}