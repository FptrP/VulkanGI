#include "resources.hpp"
#include <iostream>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "lib/stbi_image.h"

namespace drv {

  static void gen_mipmaps(Image &img, vk::CommandBuffer &cmd);

  ImageID ResourceStorage::create_image(Context &ctx, const vk::ImageCreateInfo &info, const void *pixels) {
    if (!pixels) throw std::runtime_error {"empty textures not supported"};

    Image img;
    img.info = info;
    img.info.initialLayout = vk::ImageLayout::eUndefined;
    img.layout = img.info.initialLayout;
    img.info.queueFamilyIndexCount = ctx.queue_family_count();
    img.info.pQueueFamilyIndices = ctx.get_queue_indexes();
    img.info.format = vk::Format::eR8G8B8A8Srgb;
    img.info.usage |= vk::ImageUsageFlagBits::eTransferDst;

    img.handle = ctx.get_device().createImage(img.info);
    auto rq =  ctx.get_device().getImageMemoryRequirements(img.handle);
    img.blk = memory.allocate(GPUMemoryT::Local, rq.size, rq.alignment);
    ctx.get_device().bindImageMemory(img.handle, img.blk.memory, img.blk.offset);

  
    vk::DeviceSize bufsz = img.info.extent.width * img.info.extent.height * 4;
    if (bufsz > MAX_TRANSFER_BUFFER_SIZE) {
      throw std::runtime_error {"texture is too big"};
    }

    auto staging_buf = create_buffer(ctx, GPUMemoryT::Coherent, bufsz, vk::BufferUsageFlagBits::eTransferSrc);
    buffer_memcpy(ctx, staging_buf, 0, pixels, bufsz);

    auto cmd  = begin_transfer(ctx);
    
    vk::ImageSubresourceRange img_range {};
    img_range
      .setLevelCount(1)
      .setLayerCount(1)
      .setBaseArrayLayer(0)
      .setBaseMipLevel(0)
      .setAspectMask(vk::ImageAspectFlagBits::eColor);

    vk::ImageMemoryBarrier barrier {};
    barrier.setImage(img.handle);
    barrier.setOldLayout(vk::ImageLayout::eUndefined);
    barrier.setNewLayout(vk::ImageLayout::eTransferDstOptimal);
    barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier.setSubresourceRange(img_range);

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, (vk::DependencyFlags)0, 0, nullptr, 0, nullptr, 1, &barrier);
    vk::ImageSubresourceLayers layers {};
    layers
      .setMipLevel(0)
      .setBaseArrayLayer(0)
      .setLayerCount(1)
      .setAspectMask(vk::ImageAspectFlagBits::eColor);

    vk::BufferImageCopy copy;
    copy
      .setBufferOffset(0)
      .setBufferImageHeight(img.info.extent.height)
      .setBufferRowLength(img.info.extent.width)
      .setImageExtent(img.info.extent)
      .setImageOffset({0, 0, 0})
      .setImageSubresource(layers);
    auto regions = {copy};

    cmd.copyBufferToImage(staging_buf->api_buffer(), img.handle, vk::ImageLayout::eTransferDstOptimal, regions);
    
    vk::ImageMemoryBarrier barrier2 {};
    barrier2.setImage(img.handle);
    barrier2.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
    barrier2.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    barrier2.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier2.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier2.setSubresourceRange(img_range);
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, (vk::DependencyFlags)0, 0, nullptr, 0, nullptr, 1, &barrier2);
    submit_and_wait(ctx, cmd);

    img.layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    return images.create(img);
  }

  ImageViewID ResourceStorage::create_image_view(Context &ctx, const ImageID &img, const vk::ImageViewType &t, const vk::ImageSubresourceRange &range, vk::ComponentMapping map) {
    vk::ImageSubresourceRange r = range;
    r.levelCount = min(img->get_info().mipLevels, r.levelCount);

    vk::ImageViewCreateInfo info {};
    info.setFormat(img->info.format);
    info.setImage(img->handle);
    info.setViewType(t);
    info.setSubresourceRange(r);
    info.setComponents(map);
    
    ImageView view {ctx.get_device().createImageView(info), img};
    return views.create(view);
  }

  ImageID ResourceStorage::load_image2D(Context &ctx, const char *path) {
    int t_w, t_h, t_c;
    stbi_uc *pixels = stbi_load(path, &t_w, &t_h, &t_c, STBI_rgb_alpha);

    if (!pixels) {
      std::string err; 
      err = "Image load failed : ";
      err += path;
      throw std::runtime_error {err};
    }

    u32 mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(t_w, t_h)))) + 1;

    vk::ImageCreateInfo info {};
    info
      .setArrayLayers(1)
      .setExtent({t_w, t_h, 1})
      .setFormat(vk::Format::eR8G8B8A8Srgb)
      .setImageType(vk::ImageType::e2D)
      .setMipLevels(mip_levels)
      .setInitialLayout(vk::ImageLayout::eUndefined)
      .setQueueFamilyIndexCount(ctx.queue_family_count())
      .setPQueueFamilyIndices(ctx.get_queue_indexes())
      .setSamples(vk::SampleCountFlagBits::e1)
      .setTiling(vk::ImageTiling::eOptimal)
      .setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc);
    
    auto handle = ctx.get_device().createImage(info);
    auto rq = ctx.get_device().getImageMemoryRequirements(handle);
    auto blk = memory.allocate(GPUMemoryT::Local, rq.size, rq.alignment);
    ctx.get_device().bindImageMemory(handle, blk.memory, blk.offset);

    Image img;
    img.handle = handle;
    img.blk = blk;
    img.info = info;
    img.mem_type = GPUMemoryT::Local;
    std::cout << "image " << t_w << " " << t_h << " " << t_c << "\n";

    u32 buffsz = t_w * t_h * 4;
    if (buffsz > MAX_TRANSFER_BUFFER_SIZE) {
      throw std::runtime_error {"Sequential transfer not implemented"};
    }

    auto staging_buf = create_buffer(ctx, GPUMemoryT::Coherent, buffsz, vk::BufferUsageFlagBits::eTransferSrc);
    buffer_memcpy(ctx, staging_buf, 0, pixels, buffsz);
    stbi_image_free(pixels);

    auto cmd  = begin_transfer(ctx);
    
    vk::ImageSubresourceRange img_range {};
    img_range
      .setLevelCount(mip_levels)
      .setLayerCount(1)
      .setBaseArrayLayer(0)
      .setBaseMipLevel(0)
      .setAspectMask(vk::ImageAspectFlagBits::eColor);

    vk::ImageMemoryBarrier transfer_barrier {};
    transfer_barrier.setImage(img.handle);
    transfer_barrier.setOldLayout(vk::ImageLayout::eUndefined);
    transfer_barrier.setNewLayout(vk::ImageLayout::eTransferDstOptimal);
    transfer_barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    transfer_barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    transfer_barrier.setSubresourceRange(img_range);

    cmd.pipelineBarrier(
      vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, 
      (vk::DependencyFlags)0, 0, nullptr, 0, nullptr, 1, &transfer_barrier);
    
    vk::ImageSubresourceLayers layers {};
    layers
      .setMipLevel(0)
      .setBaseArrayLayer(0)
      .setLayerCount(1)
      .setAspectMask(vk::ImageAspectFlagBits::eColor);

    vk::BufferImageCopy copy;
    copy
      .setBufferOffset(0)
      .setBufferImageHeight(img.info.extent.height)
      .setBufferRowLength(img.info.extent.width)
      .setImageExtent(img.info.extent)
      .setImageOffset({0, 0, 0})
      .setImageSubresource(layers);
    auto regions = {copy};

    cmd.copyBufferToImage(staging_buf->api_buffer(), img.handle, vk::ImageLayout::eTransferDstOptimal, regions);
    
    gen_mipmaps(img, cmd);

    vk::ImageMemoryBarrier shader_barrier {};
    shader_barrier.setImage(img.handle);
    shader_barrier.setOldLayout(vk::ImageLayout::eTransferSrcOptimal);
    shader_barrier.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    shader_barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    shader_barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    shader_barrier.setSubresourceRange(img_range);
    
    cmd.pipelineBarrier(
      vk::PipelineStageFlagBits::eTransfer,
      vk::PipelineStageFlagBits::eBottomOfPipe, 
      (vk::DependencyFlags)0, 0, nullptr, 
      0, nullptr, 1, &shader_barrier);
    
    submit_and_wait(ctx, cmd);

    img.layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    return images.create(img);    
  }

  ImageID ResourceStorage::create_depth2D_rt(Context &ctx, u32 width, u32 height) {
    Image img;
    img.info.format = vk::Format::eD24UnormS8Uint;
    img.info.imageType = vk::ImageType::e2D;
    img.info.initialLayout = vk::ImageLayout::eUndefined;
    img.info.extent = vk::Extent3D {width, height, 1};
    img.info.mipLevels = 1;
    img.info.arrayLayers = 1;
    img.info
      .setQueueFamilyIndexCount(ctx.queue_family_count())
      .setPQueueFamilyIndices(ctx.get_queue_indexes())
      .setSamples(vk::SampleCountFlagBits::e1)
      .setTiling(vk::ImageTiling::eOptimal)
      .setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eDepthStencilAttachment);
    
    img.handle = ctx.get_device().createImage(img.info);
    auto rq =  ctx.get_device().getImageMemoryRequirements(img.handle);
    img.blk = memory.allocate(GPUMemoryT::Local, rq.size, rq.alignment);
    ctx.get_device().bindImageMemory(img.handle, img.blk.memory, img.blk.offset);

    img.layout = vk::ImageLayout::eUndefined;
    img.mem_type = GPUMemoryT::Local;

    return images.create(img);
  }

  ImageViewID ResourceStorage::create_rt_view(Context &ctx, const ImageID &img, const vk::ImageAspectFlags &flags, vk::ComponentMapping map) {
    vk::ImageSubresourceRange range {};
    range
      .setBaseArrayLayer(0)
      .setBaseMipLevel(0)
      .setLayerCount(1)
      .setLevelCount(1)
      .setAspectMask(flags);

    return create_image_view(ctx, img, vk::ImageViewType::e2D, range, map);
  }

  static void gen_mipmaps(Image &img, vk::CommandBuffer &cmd) {
    auto& info = img.get_info();
    if (info.imageType != vk::ImageType::e2D) return;
    
    u32 width = info.extent.width;
    u32 height = info.extent.height;

    for (u32 lvl = 1; lvl < info.mipLevels; lvl++) {
      vk::ImageSubresourceRange from {};

      from
        .setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseArrayLayer(0)
        .setLayerCount(1)
        .setBaseMipLevel(lvl - 1)
        .setLevelCount(1);
      
      vk::ImageMemoryBarrier to_src {};
      to_src
        .setImage(img.api_image())
        .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
        .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
        .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
        .setSubresourceRange(from);

      cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        (vk::DependencyFlags)0,
        {},
        {}, {to_src});

      vk::ImageBlit region {};

      vk::ImageSubresourceLayers src_layer {}, dst_layer {};

      src_layer
        .setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseArrayLayer(0)
        .setLayerCount(1)
        .setMipLevel(lvl - 1);

      dst_layer
        .setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseArrayLayer(0)
        .setLayerCount(1)
        .setMipLevel(lvl);

      u32 w2 = max(width/2u, 1u);
      u32 h2 = max(height/2u, 1u);
      
      region
        .setSrcSubresource(src_layer)
        .setDstSubresource(dst_layer)
        .setSrcOffsets({vk::Offset3D{0, 0, 0}, vk::Offset3D{width, height, 1}})
        .setDstOffsets({vk::Offset3D{0, 0, 0}, vk::Offset3D{w2, h2, 1}});
      
      cmd.blitImage(
        img.api_image(), 
        vk::ImageLayout::eTransferSrcOptimal, 
        img.api_image(), 
        vk::ImageLayout::eTransferDstOptimal,
        {region},
        vk::Filter::eLinear);

      width = w2;
      height = h2;
    }

    vk::ImageSubresourceRange from {};
    from
        .setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseArrayLayer(0)
        .setLayerCount(1)
        .setBaseMipLevel(info.mipLevels - 1)
        .setLevelCount(1);
      
      vk::ImageMemoryBarrier to_src {};
      to_src
        .setImage(img.api_image())
        .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
        .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
        .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
        .setSubresourceRange(from);

      cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        (vk::DependencyFlags)0,
        {},
        {}, {to_src});
  }
}