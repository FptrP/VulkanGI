#include "resources.hpp"
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "lib/stbi_image.h"

namespace drv {

  ImageID ResourceStorage::create_image(Context &ctx, const vk::ImageCreateInfo &info, const void *pixels) {
    if (!pixels) throw std::runtime_error {"empty textures not supported"};

    Image img;
    img.info = info;
    img.info.initialLayout = vk::ImageLayout::eUndefined;
    img.layout = img.info.initialLayout;
    img.info.queueFamilyIndexCount = 2;
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
    vk::ImageViewCreateInfo info {};
    info.setFormat(img->info.format);
    info.setImage(img->handle);
    info.setViewType(t);
    info.setSubresourceRange(range);
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

    vk::ImageCreateInfo info {};
    info
      .setArrayLayers(1)
      .setExtent({t_w, t_h, 1})
      .setFormat(vk::Format::eR8G8B8A8Srgb)
      .setImageType(vk::ImageType::e2D)
      .setMipLevels(1)
      .setInitialLayout(vk::ImageLayout::eUndefined)
      .setQueueFamilyIndexCount(2)
      .setPQueueFamilyIndices(ctx.get_queue_indexes())
      .setSamples(vk::SampleCountFlagBits::e1)
      .setTiling(vk::ImageTiling::eOptimal)
      .setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
    
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
      .setLevelCount(1)
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
    
    vk::ImageMemoryBarrier shader_barrier {};
    shader_barrier.setImage(img.handle);
    shader_barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
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
      .setQueueFamilyIndexCount(2)
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
}