#include "resources.hpp"
#include "cmd_utils.hpp"

#include <iostream>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "lib/stbi_image.h"

namespace drv {

  static void gen_mipmaps(Image &img, vk::CommandBuffer &cmd);

  void ResourceStorage::fill_image_info(Context &ctx, const vk::ImageCreateInfo &info, Image &img) {
    img.info = info;
    img.info.initialLayout = vk::ImageLayout::eUndefined;
    img.layout = img.info.initialLayout;
    img.info.queueFamilyIndexCount = ctx.queue_family_count();
    img.info.pQueueFamilyIndices = ctx.get_queue_indexes();
    img.mem_type = GPUMemoryT::Local;

    auto api_info = static_cast<VkImageCreateInfo>(info);
    VkImage api_image;
    auto alloc_info = get_alloc_info(GPUMemoryT::Local);

    if (vmaCreateImage(allocator, &api_info, &alloc_info, &api_image, &img.allocation, nullptr) != VK_SUCCESS) {
      throw std::runtime_error {"Vma image create error"};
    }

    img.handle = api_image;
    
  }

  ImageID ResourceStorage::create_image(Context &ctx, const vk::ImageCreateInfo &info, const void *pixels) {
    if (!pixels) throw std::runtime_error {"empty textures not supported"};

    vk::ImageCreateInfo patched_info = info;
    patched_info.format = vk::Format::eR8G8B8A8Srgb;
    patched_info.usage |= vk::ImageUsageFlagBits::eTransferDst;

    Image img;
    fill_image_info(ctx, patched_info, img);

  
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
    
    Image img;
    fill_image_info(ctx, info, img);
    std::cout << "image " << t_w << " " << t_h << " " << t_c << "\n";

    u32 buffsz = t_w * t_h * 4;
    if (buffsz > MAX_TRANSFER_BUFFER_SIZE) {
      throw std::runtime_error {"Sequential transfer not implemented"};
    }

    {
      auto staging_buf = create_buffer(ctx, GPUMemoryT::Coherent, buffsz, vk::BufferUsageFlagBits::eTransferSrc);
      buffer_memcpy(ctx, staging_buf, 0, pixels, buffsz);
      stbi_image_free(pixels);

      auto cmd  = begin_transfer(ctx);

      ImageBarrier transfer_barrier {img.handle, vk::ImageAspectFlagBits::eColor};
      transfer_barrier
        .set_range(0, mip_levels)
        .change_layout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal)
        .write(cmd, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer);
      
      vk::ImageSubresourceRange img_range {};
      img_range
        .setLevelCount(mip_levels)
        .setLayerCount(1)
        .setBaseArrayLayer(0)
        .setBaseMipLevel(0)
        .setAspectMask(vk::ImageAspectFlagBits::eColor);

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

      ImageBarrier shader_barrier {img.handle, vk::ImageAspectFlagBits::eColor};
      shader_barrier
        .set_range(0, mip_levels)
        .change_layout(vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal)
        .write(cmd, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe);

      submit_and_wait(ctx, cmd);
      img.layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }

    collect_buffers();
    return images.create(img);    
  }

  ImageID ResourceStorage::create_depth2D_rt(Context &ctx, u32 width, u32 height) {
    return create_rt(ctx, width, height, vk::Format::eD24UnormS8Uint, vk::ImageUsageFlagBits::eSampled|vk::ImageUsageFlagBits::eDepthStencilAttachment);
  }

  ImageID ResourceStorage::create_rt(Context &ctx, u32 width, u32 height, vk::Format fmt, vk::ImageUsageFlags usage) {
    Image img;

    vk::ImageCreateInfo info {};
    info.format = fmt;
    info.imageType = vk::ImageType::e2D;
    info.initialLayout = vk::ImageLayout::eUndefined;
    info.extent = vk::Extent3D {width, height, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info
      .setQueueFamilyIndexCount(ctx.queue_family_count())
      .setPQueueFamilyIndices(ctx.get_queue_indexes())
      .setSamples(vk::SampleCountFlagBits::e1)
      .setTiling(vk::ImageTiling::eOptimal)
      .setUsage(usage);
    
    fill_image_info(ctx, info, img);
  
    return images.create(img);
  }

  ImageID ResourceStorage::create_cubemap(Context &ctx, u32 width, u32 height, vk::Format fmt, vk::ImageUsageFlags usage) {
    Image img;

    vk::ImageCreateInfo info {};
    info.format = fmt;
    info.imageType = vk::ImageType::e2D;
    info.initialLayout = vk::ImageLayout::eUndefined;
    info.extent = vk::Extent3D {width, height, 1};
    info.mipLevels = 1;
    info.arrayLayers = 6;
    info
      .setQueueFamilyIndexCount(ctx.queue_family_count())
      .setPQueueFamilyIndices(ctx.get_queue_indexes())
      .setSamples(vk::SampleCountFlagBits::e1)
      .setTiling(vk::ImageTiling::eOptimal)
      .setFlags(vk::ImageCreateFlagBits::eCubeCompatible)
      .setUsage(usage);
    
    fill_image_info(ctx, info, img);
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

      ImageBarrier to_src {img.api_image(), vk::ImageAspectFlagBits::eColor};
      to_src
        .access_msk(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead)
        .set_range(lvl - 1, 1)
        .change_layout(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal)
        .write(cmd, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer);
      
      u32 w2 = max(width/2u, 1u);
      u32 h2 = max(height/2u, 1u);

      BlitImage region {img.api_image(), img.api_image()};

      region
        .src_offset(vk::Offset3D{0, 0, 0}, vk::Offset3D{(i32)width, (i32)height, 1})
        .dst_offset(vk::Offset3D{0, 0, 0}, vk::Offset3D{(i32)w2, (i32)h2, 1})
        .src_subresource(vk::ImageAspectFlagBits::eColor, lvl - 1)
        .dst_subresource(vk::ImageAspectFlagBits::eColor, lvl);
      
      region.write(cmd);

      width = w2;
      height = h2;
    }

    ImageBarrier to_src {img.api_image(), vk::ImageAspectFlagBits::eColor};
    to_src
      .access_msk(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead)
      .set_range(info.mipLevels - 1, 1)
      .change_layout(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal)
      .write(cmd, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer);
    
  }

  ImageViewID ResourceStorage::create_cubemap_view(Context &ctx, const ImageID &img, 
    const vk::ImageAspectFlags &flags, u32 base_mip, u32 mips, vk::ComponentMapping map) 
  {
    vk::ImageSubresourceRange range {};
    range
      .setAspectMask(flags)
      .setBaseArrayLayer(0)
      .setLayerCount(6)
      .setBaseMipLevel(base_mip)
      .setLevelCount(mips);

    vk::ImageSubresourceRange r = range;
    r.levelCount = min(img->get_info().mipLevels, r.levelCount);

    vk::ImageViewCreateInfo info {};
    info.setFormat(img->info.format);
    info.setImage(img->handle);
    info.setViewType(vk::ImageViewType::eCube);
    info.setSubresourceRange(r);
    info.setComponents(map);
    
    ImageView view {ctx.get_device().createImageView(info), img};
    return views.create(view);
  }

  ImageID ResourceStorage::create_image2D_array(Context &ctx, 
    u32 width, u32 height, vk::Format fmt, vk::ImageUsageFlags usage, u32 layers, u32 levels) 
  {
    Image img;
    vk::ImageCreateInfo info {};
    info.format = fmt;
    info.imageType = vk::ImageType::e2D;
    info.initialLayout = vk::ImageLayout::eUndefined;
    info.extent = vk::Extent3D {width, height, 1};
    info.mipLevels = max(levels, 1u);
    info.arrayLayers = layers;
    info
      .setQueueFamilyIndexCount(ctx.queue_family_count())
      .setPQueueFamilyIndices(ctx.get_queue_indexes())
      .setSamples(vk::SampleCountFlagBits::e1)
      .setTiling(vk::ImageTiling::eOptimal)
      //.setFlags(vk::ImageCreateFlagBits::e2DArrayCompatible)
      .setUsage(usage);
    
    
    fill_image_info(ctx, info, img);
  
    return images.create(img);
  }

  ImageViewID ResourceStorage::create_2Darray_view(Context &ctx, const ImageID &img, const vk::ImageAspectFlags &flags, bool one_mip) {
    vk::ImageSubresourceRange range {};
    if (one_mip) {
      range
      .setAspectMask(flags)
      .setBaseArrayLayer(0)
      .setLayerCount(img->info.arrayLayers)
      .setBaseMipLevel(0)
      .setLevelCount(1);
    } else {
      range
      .setAspectMask(flags)
      .setBaseArrayLayer(0)
      .setLayerCount(img->info.arrayLayers)
      .setBaseMipLevel(0)
      .setLevelCount(img->info.mipLevels);
    }
    
    vk::ImageViewCreateInfo info {};
    info.setFormat(img->info.format);
    info.setImage(img->handle);
    info.setViewType(vk::ImageViewType::e2DArray);
    info.setSubresourceRange(range);
    info.setComponents(vk::ComponentMapping{});
    
    ImageView view {ctx.get_device().createImageView(info), img};
    return views.create(view);
  }

  ImageViewID ResourceStorage::create_2Darray_mip_view(Context &ctx, const ImageID &img, vk::ImageAspectFlags flags, u32 mip_level) {
    vk::ImageSubresourceRange range {};
    range
      .setAspectMask(flags)
      .setBaseArrayLayer(0)
      .setLayerCount(img->info.arrayLayers)
      .setBaseMipLevel(mip_level)
      .setLevelCount(1);
    
    vk::ImageViewCreateInfo info {};
    info.setFormat(img->info.format);
    info.setImage(img->handle);
    info.setViewType(vk::ImageViewType::e2DArray);
    info.setSubresourceRange(range);
    info.setComponents(vk::ComponentMapping{});
    
    ImageView view {ctx.get_device().createImageView(info), img};
    return views.create(view);
  }

  ImageViewID ResourceStorage::create_2Dlayer_view(Context &ctx, const ImageID &img, const vk::ImageAspectFlags &flags, u32 layer) {
    vk::ImageSubresourceRange range {};
    range
      .setAspectMask(flags)
      .setBaseArrayLayer(layer)
      .setLayerCount(1)
      .setBaseMipLevel(0)
      .setLevelCount(1);
    
    vk::ImageViewCreateInfo info {};
    info.setFormat(img->info.format);
    info.setImage(img->handle);
    info.setViewType(vk::ImageViewType::e2D);
    info.setSubresourceRange(range);
    info.setComponents(vk::ComponentMapping{});
    
    ImageView view {ctx.get_device().createImageView(info), img};
    return views.create(view);
  }
}

