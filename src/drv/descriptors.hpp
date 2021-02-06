#ifndef DESCRIPTORS_HPP_INCLUDED
#define DESCRIPTORS_HPP_INCLUDED

#include "context.hpp"

#include <list>
#include <map>


namespace drv {
  struct DescriptorStorage;

  struct DesciptorSetLayoutID {
  private:
    u32 index;
    friend DescriptorStorage;
  };

  struct DescriptorSetID;

  struct DescriptorStorage {
    DesciptorSetLayoutID create_layout(Context &ctx, const vk::DescriptorSetLayoutCreateInfo &info, u32 max_sets);
    void free_layout(Context &ctx, DesciptorSetLayoutID id);

    DescriptorSetID allocate_set(Context &ctx, DesciptorSetLayoutID layout);
    void free_set(Context &ctx, DescriptorSetID id);

    const vk::DescriptorSetLayout& get(DesciptorSetLayoutID id);
    const vk::DescriptorSet& get(DescriptorSetID id);

  private:

    struct Pool {
      vk::DescriptorSetLayout layout;
      vk::DescriptorPool desc_pool;
      
      u32 max_descriptors;
      u32 allocated;

      std::vector<vk::DescriptorSet> sets;
      std::list<u32> free_indexes;
    };

    std::map<u32, Pool> pools;
    std::list<u32> free_pools;
    u32 pools_counter = 0;
    friend DescriptorSetID;
  };

  struct DescriptorSetID {
  private:
    DescriptorSetID() {}
    
    u32 pool_index;
    u32 desc_index;

    friend DescriptorStorage;
  };

  struct DescriptorBinder {
    DescriptorBinder(const vk::DescriptorSet &set);

    DescriptorBinder &bind_ubo(u32 slot, const vk::Buffer &buf, VkDeviceSize offs = 0, vk::DeviceSize range = VK_WHOLE_SIZE);
    DescriptorBinder &bind_combined_img(u32 slot, const vk::ImageView &view, const vk::Sampler &smp, vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);
    DescriptorBinder &bind_storage_buff(u32 slot, const vk::Buffer &buf, VkDeviceSize offs = 0, vk::DeviceSize range = VK_WHOLE_SIZE);
    DescriptorBinder &bind_sampler(u32 slot, const vk::Sampler &smp);
    DescriptorBinder &bind_array_of_img(u32 slot, u32 count, const vk::ImageView *views, vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);
    DescriptorBinder &bind_input_attachment(u32 slot, const vk::ImageView &view, vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);

    void write(Context &ctx);
  private:
    vk::DescriptorSet dst;
    std::vector<vk::WriteDescriptorSet> writes;
    std::vector<std::unique_ptr<vk::DescriptorImageInfo>> images;
    std::vector<std::unique_ptr<vk::DescriptorBufferInfo>> buffers;
    std::vector<std::unique_ptr<vk::DescriptorImageInfo[]>> arrays_of_img;
  };
};

#endif