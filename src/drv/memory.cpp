#include "memory.hpp"

#include <iostream>

namespace drv {

  static vk::DeviceSize get_alignment_offs(vk::DeviceSize offset, vk::DeviceSize alignment) {
    auto mod = offset % alignment;
    return mod? (alignment - mod) : 0;  
  }

  static bool is_suitable(vk::MemoryPropertyFlags flags, GPUMemoryT type) {
    if (type == GPUMemoryT::Local) {
      return (flags & vk::MemoryPropertyFlagBits::eDeviceLocal) && !(flags & vk::MemoryPropertyFlagBits::eHostVisible);
    } else {
      return (flags & vk::MemoryPropertyFlagBits::eHostVisible) && (flags & vk::MemoryPropertyFlagBits::eHostCoherent) && (flags & vk::MemoryPropertyFlagBits::eDeviceLocal);
    }
  }

  void FreeListAllocator::init(MemoryBlock base_block) {
    free_blocks.clear();
    used_blocks.clear();

    base = base_block;
    free_blocks.push_back(base);
  }

  MemoryBlock FreeListAllocator::allocate(vk::DeviceSize size, vk::DeviceSize alignment) {
    auto alloc = free_blocks.end();
    vk::DeviceSize min_overhead = 0;

    for(auto iter = free_blocks.begin(); iter != free_blocks.end(); ++iter) {
      auto block = *iter;
      auto delta = get_alignment_offs(block.offset, alignment);
      
      if (block.size >= (size + delta)) {
        if (alloc == free_blocks.end()) {
          alloc = iter;
          min_overhead = block.size - size - delta;
          continue;
        }

        auto memory_overhead = block.size - size - delta;
        if (memory_overhead < min_overhead) {
          min_overhead = memory_overhead;
          alloc = iter;
        }
      }
    }

    if (alloc == free_blocks.end()) {
      throw std::runtime_error {"Bad GPUalloc"};
    }

    auto block = *alloc;
    free_blocks.erase(alloc);
    auto delta = get_alignment_offs(block.offset, alignment);

    MemoryBlock allocated;
    allocated.memory = block.memory;
    allocated.offset = block.offset + delta;
    allocated.size = size;

    auto end_offset = allocated.offset + size;
    auto end_size = block.size - delta - size;

    if (end_size > MIN_BLOCK_SIZE) {
      MemoryBlock rest;
      rest.memory = block.memory;
      rest.offset = end_offset;
      rest.size = end_size;
      free_blocks.push_back(rest);
    }

    used_blocks[allocated.offset] = allocated;
    return allocated;
  }
  
  bool FreeListAllocator::free(vk::DeviceSize address) {
    auto iter = used_blocks.find(address);
    if (iter == used_blocks.end()) {
      return false;
    }

    auto block = iter->second;
    used_blocks.erase(iter);
    free_blocks.push_back(block);
    fast_defrag();
    return true;
  }

  void FreeListAllocator::fast_defrag() {
    if (free_blocks.size() < 2) return;

    auto iter = free_blocks.begin();
    auto first  = *iter;
    iter++; 
    auto second = *iter;
    

    if (second.offset < first.offset) { std::swap(first, second); }

    if (first.offset + first.size == second.offset) {
      free_blocks.pop_front();
      free_blocks.pop_front();
      first.size += second.size;
      free_blocks.push_front(first);
    }
  }

  void FreeListAllocator::full_defrag() {

  }

  void GPUMemory::init(Context &ctx, vk::DeviceSize coherent_budget, vk::DeviceSize local_budget) {
    auto &dev = ctx.get_physical_device();
    auto properties = dev.getMemoryProperties();
    
    bool local_mem_found = false;
    uint32_t local_mem_type;

    for (u32 i = 0; i < properties.memoryTypeCount; i++) {
      if (is_suitable(properties.memoryTypes[i].propertyFlags, GPUMemoryT::Local)) {
        auto size = properties.memoryHeaps[properties.memoryTypes[i].heapIndex].size;
        if (size >= local_budget) {
          local_mem_found = true;
          local_mem_type = i;
          break;
        }
      }
    }

    bool coherent_mem_found = false;
    uint32_t coherent_mem_type;

    for (u32 i = 0; i < properties.memoryTypeCount; i++) {
      if (is_suitable(properties.memoryTypes[i].propertyFlags, GPUMemoryT::Coherent)) {
        auto size = properties.memoryHeaps[properties.memoryTypes[i].heapIndex].size;
        if (size >= coherent_budget) {
          coherent_mem_found = true;
          coherent_mem_type = i;
          break;
        }
      }
    }

    if (!coherent_mem_found || !local_mem_found) {
      throw std::runtime_error {"Suitable memory not found"};
    }

    coherent_type = coherent_mem_type;
    local_type = local_mem_type;


    vk::MemoryAllocateInfo info {};
    info.setAllocationSize(local_budget);
    info.setMemoryTypeIndex(local_type);

    local_blk.memory = ctx.get_device().allocateMemory(info);
    local_blk.offset = 0;
    local_blk.size = local_budget;

    info.setMemoryTypeIndex(coherent_type);
    info.setAllocationSize(coherent_budget);

    coherent_blk.memory = ctx.get_device().allocateMemory(info);
    coherent_blk.offset = 0;
    coherent_blk.size = coherent_budget;

    coherent_pool.init(coherent_blk);
    local_pool.init(local_blk);
  }   

  void GPUMemory::release(Context &ctx) {
    ctx.get_device().freeMemory(coherent_blk.memory);
    ctx.get_device().freeMemory(local_blk.memory);
  }

  MemoryBlock GPUMemory::allocate(GPUMemoryT type, vk::DeviceSize size, vk::DeviceSize alignment) {
    return ((type == GPUMemoryT::Local)? (&local_pool) : (&coherent_pool))->allocate(size, alignment);
  }

  bool GPUMemory::free(GPUMemoryT type, vk::DeviceSize address) {
    return ((type == GPUMemoryT::Local)? (&local_pool) : (&coherent_pool))->free(address);
  }

  bool GPUMemory::free(MemoryBlock blk) {    
    if (blk.memory == local_blk.memory) {
      return local_pool.free(blk.offset);
    } else if (blk.memory == coherent_blk.memory) {
      return coherent_pool.free(blk.offset);
    }
    
    return false;
  }

}