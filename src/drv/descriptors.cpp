#include "descriptors.hpp"
#include <iostream>
namespace drv {

  DesciptorSetLayoutID DescriptorStorage::create_layout(Context &ctx, const vk::DescriptorSetLayoutCreateInfo &info, u32 max_sets) {
    u32 alloc_index;
    if (free_pools.size()) {
      alloc_index = free_pools.front();
      free_pools.pop_front();
    } else {
      alloc_index = ++pools_counter;
    }

    auto layout = ctx.get_device().createDescriptorSetLayout(info);

    vk::DescriptorPoolCreateInfo pool_info {};
    pool_info.setMaxSets(max_sets);
    
    std::vector<vk::DescriptorPoolSize> sizes;
    
    for (u32 i = 0; i < info.bindingCount; i++) {
      vk::DescriptorPoolSize *ptr = nullptr;
      for (u32 j = 0; j < sizes.size(); j++) {
        if (sizes[j].type == info.pBindings[i].descriptorType) {
          ptr = &sizes[j];
        }
      }

      if (!ptr) {
        sizes.push_back({});
        ptr = &sizes[sizes.size() - 1];
        ptr->type = info.pBindings[i].descriptorType;
      }

      ptr->descriptorCount += info.pBindings[i].descriptorCount;

    }

    for (u32 i = 0; i < sizes.size(); i++) {
      sizes[i].descriptorCount *= max_sets;
    }
    pool_info.setPoolSizes(sizes);
    
    auto pool = ctx.get_device().createDescriptorPool(pool_info);

    pools[alloc_index].allocated = 0;
    pools[alloc_index].desc_pool = pool;
    pools[alloc_index].layout = layout;
    pools[alloc_index].max_descriptors = max_sets;

    DesciptorSetLayoutID id;
    id.index = alloc_index;
    return id;
  }

  void DescriptorStorage::free_layout(Context &ctx, DesciptorSetLayoutID id) {
    auto cell = pools.at(id.index);
    ctx.get_device().destroyDescriptorPool(cell.desc_pool);
    ctx.get_device().destroyDescriptorSetLayout(cell.layout);
    free_pools.push_front(id.index); 
  }

  DescriptorSetID DescriptorStorage::allocate_set(Context &ctx, DesciptorSetLayoutID layout) {
    auto &cell = pools.at(layout.index);
    if (cell.allocated >= cell.max_descriptors) {
      throw std::runtime_error {"Descriptor pool overflow"};
    }

    u32 index; 
    if (cell.free_indexes.size()) {
      index = cell.free_indexes.front();
      cell.free_indexes.pop_front();
    } else {
      cell.sets.push_back({});
      index = cell.sets.size() - 1;
    }

    vk::DescriptorSetAllocateInfo info {};
    info.setDescriptorPool(cell.desc_pool);
    
    auto layouts = { cell.layout };
    info.setDescriptorSetCount(1);
    info.setSetLayouts(layouts);

    auto desc = ctx.get_device().allocateDescriptorSets(info).at(0);

    cell.sets[index] = desc;
    cell.allocated++;

    DescriptorSetID id;
    id.pool_index = layout.index;
    id.desc_index = index;
    return id;
  }

  void DescriptorStorage::free_set(Context &ctx, DescriptorSetID id) {
    auto &cell = pools.at(id.pool_index);
    auto to_free = {cell.sets[id.desc_index]};
    ctx.get_device().freeDescriptorSets(cell.desc_pool, to_free);

    cell.allocated--;
    cell.free_indexes.push_front(id.desc_index);
  }

  const vk::DescriptorSetLayout& DescriptorStorage::get(DesciptorSetLayoutID id) {
    return pools.at(id.index).layout;
  }

  const vk::DescriptorSet& DescriptorStorage::get(DescriptorSetID id) {
    return pools.at(id.pool_index).sets.at(id.desc_index);
  }

  DescriptorBinder::DescriptorBinder(const vk::DescriptorSet &set) : dst{set} {}

  DescriptorBinder &DescriptorBinder::bind_ubo(u32 slot, const vk::Buffer &buf, VkDeviceSize offs, vk::DeviceSize range) {
    vk::DescriptorBufferInfo info {};
    info
      .setBuffer(buf)
      .setOffset(offs)
      .setRange(range);
    
    buffers.push_back(std::unique_ptr<vk::DescriptorBufferInfo>{new vk::DescriptorBufferInfo{info}});

    vk::WriteDescriptorSet write {};
    write
      .setDstSet(dst)
      .setDstBinding(slot)
      .setDescriptorType(vk::DescriptorType::eUniformBuffer)
      .setDescriptorCount(1)
      .setPBufferInfo(buffers[buffers.size() - 1].get());
    
    writes.push_back(write);
    return *this;
  }

  DescriptorBinder &DescriptorBinder::bind_storage_buff(u32 slot, const vk::Buffer &buf, VkDeviceSize offs, vk::DeviceSize range) {
    vk::DescriptorBufferInfo info {};
    info
      .setBuffer(buf)
      .setOffset(offs)
      .setRange(range);
    
    buffers.push_back(std::unique_ptr<vk::DescriptorBufferInfo>{new vk::DescriptorBufferInfo{info}});

    vk::WriteDescriptorSet write {};
    write
      .setDstSet(dst)
      .setDstBinding(slot)
      .setDescriptorType(vk::DescriptorType::eStorageBuffer)
      .setDescriptorCount(1)
      .setPBufferInfo(buffers[buffers.size() - 1].get());
    
    writes.push_back(write);
    return *this;
  }
  
  DescriptorBinder &DescriptorBinder::bind_combined_img(u32 slot, const vk::ImageView &view, const vk::Sampler &smp, vk::ImageLayout layout) {
    vk::DescriptorImageInfo info {};
    info
      .setSampler(smp)
      .setImageView(view)
      .setImageLayout(layout);

    images.push_back(std::unique_ptr<vk::DescriptorImageInfo>{new vk::DescriptorImageInfo{info}});
    
    vk::WriteDescriptorSet write {};
    write
      .setDstSet(dst)
      .setDstBinding(slot)
      .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
      .setDescriptorCount(1)
      .setPImageInfo(images[images.size() - 1].get());
    
    writes.push_back(write);
    return *this;
  }

  DescriptorBinder &DescriptorBinder::bind_sampler(u32 slot, const vk::Sampler &smp) {
    vk::WriteDescriptorSet write {};
    vk::DescriptorImageInfo info {};
    info.setSampler(smp);
    images.push_back(std::unique_ptr<vk::DescriptorImageInfo>{new vk::DescriptorImageInfo{info}});
    write
      .setDstSet(dst)
      .setDstBinding(slot)
      .setDescriptorType(vk::DescriptorType::eSampler)
      .setDescriptorCount(1)
      .setPImageInfo(images[images.size() - 1].get());
    
    writes.push_back(write);
    return *this;
  }

  DescriptorBinder &DescriptorBinder::bind_array_of_img(u32 slot, u32 count, const vk::ImageView *views, vk::ImageLayout layout) {
    auto data = new vk::DescriptorImageInfo[count] {};

    for (u32 i = 0; i < count; i++) {
      data[i]
        .setImageLayout(layout)
        .setImageView(views[i]);
    }

    arrays_of_img.push_back(std::unique_ptr<vk::DescriptorImageInfo[]>{data});
    vk::WriteDescriptorSet write {};
    write
      .setDstSet(dst)
      .setDstBinding(slot)
      .setDescriptorType(vk::DescriptorType::eSampledImage)
      .setDescriptorCount(count)
      .setPImageInfo(data);
    
    writes.push_back(write);
    return *this;
  }

  DescriptorBinder &DescriptorBinder::bind_input_attachment(u32 slot, const vk::ImageView &view, vk::ImageLayout layout) {
    vk::DescriptorImageInfo info {};
    info
      .setImageView(view)
      .setImageLayout(layout);

    images.push_back(std::unique_ptr<vk::DescriptorImageInfo>{new vk::DescriptorImageInfo{info}});
    
    vk::WriteDescriptorSet write {};
    write
      .setDstSet(dst)
      .setDstBinding(slot)
      .setDescriptorType(vk::DescriptorType::eInputAttachment)
      .setDescriptorCount(1)
      .setPImageInfo(images[images.size() - 1].get());
    
    writes.push_back(write);
    return *this;
  }

  void DescriptorBinder::write(Context &ctx) {
    ctx.get_device().updateDescriptorSets(writes, {});
  }

}