#ifndef RESOURCES_HPP_INCLUDED
#define RESOURCES_HPP_INCLUDED

#include "context.hpp"
#include "memory.hpp"
#include "rcstorage.hpp"

#include "lib/vk_mem_alloc.h"

namespace drv {

  #include <list>

  const u32 MAX_TRANSFER_BUFFER_SIZE = 8u << 20u;
  struct ResourceStorage;

  struct Buffer {
    
    operator vk::Buffer&() { return handle; }
    operator const vk::Buffer&() const { return handle; }
  
    VmaAllocation get_allocation() const { return allocation; }
    GPUMemoryT get_memory_type() const { return mem_type; }

    void release(VmaAllocator allocator) {
      vmaDestroyBuffer(allocator, handle, allocation);
    }

    const vk::Buffer& api_buffer() const { return handle; }

  private:
    
    VmaAllocation allocation;
    GPUMemoryT mem_type;

    vk::BufferUsageFlags usage;
    vk::SharingMode sharing_mode;
    vk::Buffer handle;

    friend ResourceStorage;
  };

  struct Image {
    VmaAllocation get_allocation() const { return allocation; }
    GPUMemoryT get_memory_type() const { return mem_type; }

    void release(VmaAllocator allocator) {
      vmaDestroyImage(allocator, handle, allocation);
    }

    const vk::Image& api_image() const { return handle; }
    vk::Image& api_image() { return handle; }
    const vk::ImageCreateInfo &get_info() const { return info; }
  private:
    VmaAllocation allocation;
    GPUMemoryT mem_type;

    vk::ImageCreateInfo info {};
    vk::Image handle;
    vk::ImageLayout layout;
    
    friend ResourceStorage;
  };

  using BufferID = RCId<Buffer>;
  using ImageID = RCId<Image>;

  struct ImageView {
    const vk::ImageView &api_view() const { return view; }
    
    void release(Context &ctx) {
      ctx.get_device().destroyImageView(view);
      img.release();
    }

    u32 img_index() const { return img.debug_index(); }

    const ImageID &get_base_img() const { return img; }
    ImageID &get_base_img() { return img; }
    
  private:
    ImageView(const vk::ImageView &v, const ImageID &i) : view{v}, img{i} {}

    vk::ImageView view;
    ImageID img;

    friend ResourceStorage;
  };

  using ImageViewID = RCId<ImageView>;

  struct ResourceStorage {
    

    void init(Context &ctx);
    void release(Context &ctx);


    BufferID create_buffer(Context &ctx, GPUMemoryT type, 
                           vk::DeviceSize size, vk::BufferUsageFlags usage,
                           vk::SharingMode mode = vk::SharingMode::eConcurrent);
    void* map_buffer(Context &ctx, const BufferID &id);
    void unmap_buffer(Context &ctx, const BufferID &id);
    void buffer_memcpy(Context &ctx, const BufferID &dst, vk::DeviceSize offst, const void *src, vk::DeviceSize size);
    void collect_buffers();
    Buffer &get(BufferID &id);
    const Buffer &get(const BufferID &id) const;

    ImageID create_image(Context &ctx, const vk::ImageCreateInfo &info, const void *pixels = nullptr);
    ImageID load_image2D(Context &ctx, const char *path);
    ImageID create_depth2D_rt(Context &ctx, u32 width, u32 height);
    ImageID create_rt(Context &ctx, u32 width, u32 height, vk::Format fmt, vk::ImageUsageFlags usage);
    ImageID create_cubemap(Context &ctx, u32 width, u32 height, vk::Format fmt, vk::ImageUsageFlags usage);
    ImageID create_image2D_array(Context &ctx, u32 width, u32 height, vk::Format fmt, vk::ImageUsageFlags usage, u32 layers, u32 levels = 1);

    ImageViewID create_image_view(Context &ctx, const ImageID &img, const vk::ImageViewType &t, const vk::ImageSubresourceRange &range, vk::ComponentMapping map = {});
    ImageViewID create_rt_view(Context &ctx, const ImageID &img, const vk::ImageAspectFlags &flags, vk::ComponentMapping map = {});
    ImageViewID create_cubemap_view(Context &ctx, const ImageID &img, 
      const vk::ImageAspectFlags &flags, u32 base_mip = 0, u32 mips = 1, vk::ComponentMapping map = {});
    ImageViewID create_2Darray_view(Context &ctx, const ImageID &img, const vk::ImageAspectFlags &flags, bool one_mip=true);
    ImageViewID create_2Darray_mip_view(Context &ctx, const ImageID &img, vk::ImageAspectFlags flags, u32 mip_level = 0);
    ImageViewID create_2Dlayer_view(Context &ctx, const ImageID &img, const vk::ImageAspectFlags &flags, u32 layer);

  private: 

    void fill_image_info(Context &ctx, const vk::ImageCreateInfo &info, Image &img);

    VmaAllocationCreateInfo get_alloc_info(GPUMemoryT type) const {
      VmaAllocationCreateInfo info {};
      if (type == GPUMemoryT::Local) {
        info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
      }
      if (type == GPUMemoryT::Coherent) {
        info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
      }
      return info;
    }

    vk::CommandBuffer begin_transfer(Context &ctx);
    void submit_and_wait(Context &ctx, vk::CommandBuffer &cmd);

    void buffer_memcpy_coherent(Context &ctx, const BufferID &dst, vk::DeviceSize offst, const void *src, vk::DeviceSize size);
    void buffer_memcpy_local(Context &ctx, const BufferID &dst, vk::DeviceSize offst, const void *src, vk::DeviceSize size);

    VmaAllocator allocator;
    vk::CommandPool cmd_pool;
    RCStorage<Buffer> buffers;
    RCStorage<Image> images;
    RCStorage<ImageView> views;
  };

}

#endif