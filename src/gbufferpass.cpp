#include "triangle.hpp"

void GBufferSubpass::create_texture_sets(DriverState &ds) {
  vk::ImageSubresourceRange i_range {};
  i_range
    .setAspectMask(vk::ImageAspectFlagBits::eColor)
    .setBaseArrayLayer(0)
    .setBaseMipLevel(0)
    .setLayerCount(1)
    .setLevelCount(~0u);
    
  for (auto &mat : scene.get_materials()) {
    if (mat.albedo_path.empty()) continue;
    auto img = ds.storage.load_image2D(ds.ctx, mat.albedo_path.c_str());
    auto view = ds.storage.create_image_view(ds.ctx, img, vk::ImageViewType::e2D, i_range);
    images.push_back(view);
  }

  drv::DescriptorSetLayoutBuilder builder {};
  builder
    .add_sampler(0, vk::ShaderStageFlagBits::eFragment)
    .add_array_of_tex(1, images.size(), vk::ShaderStageFlagBits::eFragment);
    
  tex_layout = ds.descriptors.create_layout(ds.ctx, builder.build(), 1);
  texture_set = ds.descriptors.allocate_set(ds.ctx, tex_layout);
}