#include "resources.hpp"
#include <iostream>

#define VMA_IMPLEMENTATION
#include "lib/vk_mem_alloc.h"


namespace drv {

  #define VMA_CHECK(exp, msg) if ((exp) != VK_SUCCESS) throw std::runtime_error{msg}

  BufferID ResourceStorage::create_buffer(Context &ctx, GPUMemoryT type, 
                           vk::DeviceSize size, vk::BufferUsageFlags usage,
                           vk::SharingMode mode)
  {
    vk::BufferCreateInfo info {};
    
    if (ctx.queue_family_count() == 1) {
      mode = vk::SharingMode::eExclusive;
    }

    info.setSharingMode(mode);
    info.setUsage(usage);
    info.setSize(size);
    info.setPQueueFamilyIndices(ctx.get_queue_indexes());
    info.setQueueFamilyIndexCount(ctx.queue_family_count());

    auto allocation_info = get_alloc_info(type);
    auto raw_info = static_cast<VkBufferCreateInfo>(info);

    VkBuffer handle;
    VmaAllocation allocation;
    VMA_CHECK(vmaCreateBuffer(allocator, &raw_info, &allocation_info, &handle, &allocation, nullptr), "Buffer create error");

    Buffer cell;
    cell.allocation = allocation;
    cell.handle = handle;
    cell.mem_type = type;
    cell.sharing_mode = mode;
    cell.usage = usage;

    return buffers.create(cell);
  }

  void ResourceStorage::collect_buffers() {
    buffers.collect(allocator);
  }

  void* ResourceStorage::map_buffer(Context &ctx, const BufferID &id) {

    void *ptr = nullptr;
    
    VMA_CHECK(vmaMapMemory(allocator, id->get_allocation(), &ptr), "Vma map memory error");
    return ptr;
  }

  void ResourceStorage::unmap_buffer(Context &ctx, const BufferID &id) {
    vmaUnmapMemory(allocator, id->get_allocation());
  }

  Buffer& ResourceStorage::get(BufferID &id) {
    return (*id);
  }

  void ResourceStorage::init(Context &ctx) {

    {
      VmaAllocatorCreateInfo info {};
      info.device = static_cast<VkDevice>(ctx.get_device());
      info.physicalDevice = static_cast<VkPhysicalDevice>(ctx.get_physical_device());
      info.instance = static_cast<VkInstance>(ctx.get_instance());
      info.vulkanApiVersion = VK_API_VERSION_1_2;

      auto result = vmaCreateAllocator(&info, &allocator);
      if (result != VK_SUCCESS) {
        throw std::runtime_error {"VMA allocator create error"};
      }
    }

    vk::CommandPoolCreateInfo info {};
    info.setQueueFamilyIndex(ctx.queue_index(QueueT::Transfer));

    cmd_pool = ctx.get_device().createCommandPool(info);
  }

  void ResourceStorage::release(Context &ctx) {
    ctx.get_device().destroyCommandPool(cmd_pool);

    views.collect(ctx);
    collect_buffers();
    images.collect(allocator);

    vmaDestroyAllocator(allocator);
  }

  vk::CommandBuffer ResourceStorage::begin_transfer(Context &ctx) {
    vk::CommandBufferAllocateInfo info {};
    info.setCommandBufferCount(1);
    info.setCommandPool(cmd_pool);
    info.setLevel(vk::CommandBufferLevel::ePrimary);
    
    auto res = ctx.get_device().allocateCommandBuffers(info);
    auto cmd = res.at(0);
    vk::CommandBufferBeginInfo begin {};
    begin.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    cmd.begin(begin);
    return cmd;
  }
  
  void ResourceStorage::submit_and_wait(Context &ctx, vk::CommandBuffer &cmd) {
    cmd.end();
    auto cmd_buffers = {cmd}; 
    
    vk::SubmitInfo info {};
    info.setCommandBuffers(cmd);
    
    ctx.get_queue(QueueT::Transfer).submit(info);
    ctx.get_queue(QueueT::Transfer).waitIdle();

    ctx.get_device().freeCommandBuffers(cmd_pool, cmd_buffers);
  }

  void ResourceStorage::buffer_memcpy(Context &ctx, const BufferID &dst, vk::DeviceSize offst, const void *src, vk::DeviceSize size) {
    auto &cell = *dst;

    VmaAllocationInfo info {};
    vmaGetAllocationInfo(allocator, dst->get_allocation(), &info);

    if (info.size < offst + size) {
      throw std::runtime_error {"Bad memory write"};
    }

    if (cell.mem_type == GPUMemoryT::Coherent) {
      buffer_memcpy_coherent(ctx, dst, offst, src, size);
    } else {
      buffer_memcpy_local(ctx, dst, offst, src, size);
      collect_buffers();
    }
  }

  void ResourceStorage::buffer_memcpy_coherent(Context &ctx, const BufferID &dst, vk::DeviceSize offst, const void *src, vk::DeviceSize size) {
    const auto& cell = *dst;

    char  *ptr = static_cast<char*>(map_buffer(ctx, dst)); 
    std::memcpy(ptr + offst, src, size);
    unmap_buffer(ctx, dst);
  }

  void ResourceStorage::buffer_memcpy_local(Context &ctx, const BufferID &dst, vk::DeviceSize offst, const void *src, vk::DeviceSize size) {
    auto ptr = (const u8*)src;
    
    auto staging_size = (size < MAX_TRANSFER_BUFFER_SIZE)? size : MAX_TRANSFER_BUFFER_SIZE; 
    auto staging_buff = create_buffer(
      ctx, 
      GPUMemoryT::Coherent, 
      staging_size, 
      vk::BufferUsageFlagBits::eTransferSrc,
      vk::SharingMode::eExclusive);

    vk::BufferCopy cp {};

    for (vk::DeviceSize transfer_offst = 0; transfer_offst < size; transfer_offst += staging_size) {      
      auto copy_size = (transfer_offst + staging_size <= size)? staging_size : (size - transfer_offst);
      buffer_memcpy(ctx, staging_buff, 0, ptr + transfer_offst, copy_size);

      cp.setSrcOffset(0);
      cp.setDstOffset(offst + transfer_offst);
      cp.setSize(copy_size);
            
      auto cmd = begin_transfer(ctx);
      cmd.copyBuffer(*staging_buff, *dst, 1, &cp);
      submit_and_wait(ctx, cmd);
    }
  }
}