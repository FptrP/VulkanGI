#include "resources.hpp"
#include <iostream>
namespace drv {


  BufferID ResourceStorage::create_buffer(Context &ctx, GPUMemoryT type, 
                           vk::DeviceSize size, vk::BufferUsageFlags usage,
                           vk::SharingMode mode)
  {
    vk::BufferCreateInfo info {};
    auto used_queues = { ctx.queue_index(QueueT::Transfer), ctx.queue_index(QueueT::Graphics) };
    
    info.setSharingMode(mode);
    info.setUsage(usage);
    info.setSize(size);
    info.setQueueFamilyIndices(used_queues);

    auto buffer = ctx.get_device().createBuffer(info);

    auto mem_info = ctx.get_device().getBufferMemoryRequirements(buffer);
    auto blk = memory.allocate(type, mem_info.size, mem_info.alignment);

    ctx.get_device().bindBufferMemory(buffer, blk.memory, blk.offset);

    Buffer cell;
    cell.blk = blk;
    cell.handle = buffer;
    cell.mem_type = type;
    cell.sharing_mode = mode;
    cell.usage = usage;

    return buffers.create(cell);
  }

  void ResourceStorage::collect_buffers(Context &ctx) {
    buffers.collect(ctx, memory);
  }

  void* ResourceStorage::map_buffer(Context &ctx, const BufferID &id) {
    const auto &blk = (*id).blk;

    return ctx.get_device().mapMemory(blk.memory, blk.offset, blk.size);
  }

  void ResourceStorage::unmap_buffer(Context &ctx, const BufferID &id) {
    ctx.get_device().unmapMemory((*id).blk.memory);
  }

  Buffer& ResourceStorage::get(BufferID &id) {
    return (*id);
  }

  void ResourceStorage::init(Context &ctx) {
    memory.init(ctx, 32u<<20u, 128u<<20u);

    vk::CommandPoolCreateInfo info {};
    info.setQueueFamilyIndex(ctx.queue_index(QueueT::Transfer));

    cmd_pool = ctx.get_device().createCommandPool(info);
  }

  void ResourceStorage::release(Context &ctx) {
    ctx.get_device().destroyCommandPool(cmd_pool);

    views.collect(ctx);
    collect_buffers(ctx);
    images.collect(ctx, memory);
    memory.release(ctx);
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
    if (cell.blk.size < offst + size) {
      throw std::runtime_error {"Bad memory write"};
    }

    if (cell.mem_type == GPUMemoryT::Coherent) {
      buffer_memcpy_coherent(ctx, dst, offst, src, size);
    } else {
      buffer_memcpy_local(ctx, dst, offst, src, size);
      collect_buffers(ctx);
    }
  }

  void ResourceStorage::buffer_memcpy_coherent(Context &ctx, const BufferID &dst, vk::DeviceSize offst, const void *src, vk::DeviceSize size) {
    const auto& cell = *dst;
    auto ptr = ctx.get_device().mapMemory(cell.blk.memory, cell.blk.offset + offst, cell.blk.size - offst);
    std::memcpy(ptr, src, size);
    ctx.get_device().unmapMemory(cell.blk.memory);
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