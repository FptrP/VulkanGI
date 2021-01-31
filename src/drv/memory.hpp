#ifndef MEMORY_HPP_INCLUDED
#define MEMORY_HPP_INCLUDED

#include "context.hpp"

#include <list>
#include <map>
#include <optional>

namespace drv {

  enum class GPUMemoryT {
    Coherent,
    Local 
  };

  //block of cells [offset, offset + size]
  struct MemoryBlock {
    vk::DeviceMemory memory;
    vk::DeviceSize offset;
    vk::DeviceSize size;
  };

  const vk::DeviceSize MIN_BLOCK_SIZE = 32;

  struct FreeListAllocator {
    void init(MemoryBlock base_block);

    MemoryBlock allocate(vk::DeviceSize size, vk::DeviceSize alignment);
    bool free(vk::DeviceSize address);

  private:
    std::map<vk::DeviceSize, MemoryBlock> used_blocks;
    std::list<MemoryBlock> free_blocks;
    MemoryBlock base;
  };

  struct GPUMemory {
    void init(Context &ctx, vk::DeviceSize coherent_budget, vk::DeviceSize local_budget);
    void release(Context &ctx);

    MemoryBlock allocate(GPUMemoryT type, vk::DeviceSize size, vk::DeviceSize alignment);
    bool free(GPUMemoryT type, vk::DeviceSize address);
    bool free(MemoryBlock blk);

  private:
    MemoryBlock coherent_blk, local_blk;
    FreeListAllocator coherent_pool, local_pool;

    u32 coherent_type, local_type;
  };
}

#endif