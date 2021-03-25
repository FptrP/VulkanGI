#include "scene.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <cassert>
#include <iostream>

#include "cubemap_shadow.hpp"
#include "postprocessing.hpp"


void Scene::load(const std::string &path, const std::string &folder) {
  Assimp::Importer importer {};
  auto aiscene = importer.ReadFile(path, aiProcess_GenSmoothNormals|aiProcess_Triangulate| aiProcess_SortByPType | aiProcess_FlipUVs);
  model_path = folder;

  process_materials(aiscene);
  process_meshes(aiscene);
  process_objects(aiscene->mRootNode, glm::identity<glm::mat4>());

  std::cout << "Total " << objects.size() << " objects\n";
}

void Scene::process_meshes(const aiScene *scene) {
  const u32 meshes_count = scene->mNumMeshes;
  
  u32 verts_count = 0, index_count = 0;

  for (u32 i = 0; i < scene->mNumMeshes; i++) {
    const auto &mesh = scene->mMeshes[i];
    verts_count += mesh->mNumVertices;
    index_count += 3 * mesh->mNumFaces;
  }

  meshes.reserve(meshes_count);
  verts.reserve(verts_count);
  indexes.reserve(index_count);

  for (u32 i = 0; i < scene->mNumMeshes; i++) {
    const auto &scene_mesh = scene->mMeshes[i];
    
    SceneMesh mesh;  
    mesh.material = scene_mesh->mMaterialIndex;
    mesh.vertex_offset = verts.size();
    mesh.index_offset = indexes.size();
    mesh.index_count = scene_mesh->mNumFaces * 3;
    meshes.push_back(mesh);

    for (u32 j = 0; j < scene_mesh->mNumVertices; j++) {
      SceneVertex vertex;
      vertex.pos = {scene_mesh->mVertices[j].x, scene_mesh->mVertices[j].y, scene_mesh->mVertices[j].z};
      vertex.norm = {scene_mesh->mNormals[j].x, scene_mesh->mNormals[j].y, scene_mesh->mNormals[j].z};
      vertex.uv = {scene_mesh->mTextureCoords[0][j].x, scene_mesh->mTextureCoords[0][j].y};
      verts.push_back(vertex);
    }

    for (u32 j = 0; j < scene_mesh->mNumFaces; j++) {
      assert(scene_mesh->mFaces[j].mNumIndices == 3);
      auto i1 = scene_mesh->mFaces[j].mIndices[0];
      auto i2 = scene_mesh->mFaces[j].mIndices[1];
      auto i3 = scene_mesh->mFaces[j].mIndices[2];
      
      indexes.push_back(i1);
      indexes.push_back(i2);
      indexes.push_back(i3);
    }
  }

  std::cout << "Total " << meshes_count << " meshes, " << verts_count << " vertices " << index_count << " indexes\n";
}

void Scene::process_materials(const aiScene *scene) {
  materials.reserve(scene->mNumMaterials);
  std::cout << "Materials count " << scene->mNumMaterials << "\n";
  for (u32 i = 0; i < scene->mNumMaterials; i++) {
    const auto &scene_mt = scene->mMaterials[i];
    u32 count = scene_mt->GetTextureCount(aiTextureType_DIFFUSE);
    SceneMaterialDesc mat {};

    if (count) {
      aiString path;
      scene_mt->GetTexture(aiTextureType_DIFFUSE, 0, &path);
      mat.albedo_path = path.C_Str();
      
      if (mat.albedo_path.length()) {
        mat.albedo_path = model_path + mat.albedo_path;
      }

      path.Clear();

      scene_mt->GetTexture(aiTextureType_UNKNOWN, 0, &path);
      mat.mr_path = path.C_Str();
      
      if (mat.mr_path.length()) {
        mat.mr_path = model_path + mat.mr_path;
      }
    } else {
      std::cout << "No textures\n";
    }

    materials.push_back(mat);
  }

  for (u32 i = 0; i < materials.size(); i++) {
    //std::cout << "(" << materials[i].mr_path << "\n";
  } 
}

void Scene::process_objects(const aiNode *node, glm::mat4 transform) {
  if (!node) return;

  glm::mat4 m;
  m[0] = glm::vec4{node->mTransformation.a1, node->mTransformation.b1, node->mTransformation.c1, node->mTransformation.d1};
  m[1] = glm::vec4{node->mTransformation.a2, node->mTransformation.b2, node->mTransformation.c2, node->mTransformation.d2};
  m[2] = glm::vec4{node->mTransformation.a3, node->mTransformation.b3, node->mTransformation.c3, node->mTransformation.d3};
  m[3] = glm::vec4{node->mTransformation.a4, node->mTransformation.b4, node->mTransformation.c4, node->mTransformation.d4};

  m = m * transform;

  matrices.push_back(m);
  const u32 mat_index = matrices.size() - 1;

  for (u32 i = 0; i < node->mNumMeshes; i++) {
    auto &mesh = meshes[node->mMeshes[i]];
    
    SceneObject obj;
    obj.index_offset = mesh.index_offset;
    obj.material_index = mesh.material;
    obj.matrix_index = mat_index;
    obj.vertex_offset = mesh.vertex_offset;
    obj.index_count = mesh.index_count;
    objects.push_back(obj);
  }

  for (u32 i = 0; i < node->mNumChildren; i++) {
    process_objects(node->mChildren[i], m);
  }
}

void Scene::gen_buffers(DriverState &ds) {
  const u32 verts_size = verts.size() * sizeof(SceneVertex);
  verts_buff = ds.storage.create_buffer(
    ds.ctx,
    drv::GPUMemoryT::Local,
    verts_size,
    vk::BufferUsageFlagBits::eVertexBuffer|vk::BufferUsageFlagBits::eTransferDst);
  
  ds.storage.buffer_memcpy(ds.ctx, verts_buff, 0, verts.data(), verts_size);

  
  index_buff = ds.storage.create_buffer(
    ds.ctx,
    drv::GPUMemoryT::Local,
    indexes.size() * sizeof(u32),
    vk::BufferUsageFlagBits::eIndexBuffer|vk::BufferUsageFlagBits::eTransferDst);

  ds.storage.buffer_memcpy(ds.ctx, index_buff, 0, indexes.data(), indexes.size() * sizeof(u32));

  matrix_buff = ds.storage.create_buffer(
    ds.ctx,
    drv::GPUMemoryT::Local,
    matrices.size() * sizeof(glm::mat4),
    vk::BufferUsageFlagBits::eStorageBuffer|vk::BufferUsageFlagBits::eTransferDst);
  
  ds.storage.buffer_memcpy(ds.ctx, matrix_buff, 0, matrices.data(), matrices.size() * sizeof(glm::mat4));

  std::cout << verts_size << " VB bytes\n";
  std::cout << indexes.size() * sizeof(u32) << " IB bytes\n";
  std::cout << matrices.size() * sizeof(glm::mat4) << " MB bytes\n";
}

void Scene::gen_shadows(DriverState &ds) {
  vk::Sampler sampler;

  {
    vk::SamplerCreateInfo info {};
    info.setMagFilter(vk::Filter::eLinear);
    info.setMinFilter(vk::Filter::eLinear);
    info.setMipmapMode(vk::SamplerMipmapMode::eNearest);

    sampler = ds.ctx.get_device().createSampler(info);
  }

  CubemapShadowRenderer renderer {};
  renderer.init(ds);
  const auto flags = vk::ImageUsageFlagBits::eTransferDst|vk::ImageUsageFlagBits::eSampled;
  
  auto cubemap = ds.storage.create_cubemap(ds.ctx, 256, 256, vk::Format::eR32Sfloat, flags);
  auto view = ds.storage.create_cubemap_view(ds.ctx, cubemap, vk::ImageAspectFlagBits::eColor);

  auto oct_shadows_img = ds.storage.create_image2D_array(ds.ctx, 1024, 1024, vk::Format::eR32Sfloat, vk::ImageUsageFlagBits::eSampled|vk::ImageUsageFlagBits::eColorAttachment, scene_lights.size());
  oct_shadows_array = ds.storage.create_2Darray_view(ds.ctx, oct_shadows_img, vk::ImageAspectFlagBits::eColor);


  ds.pipelines.load_shader(ds.ctx, "pass_vs", "src/shaders/pass_vert.spv", vk::ShaderStageFlagBits::eVertex);
  ds.pipelines.load_shader(ds.ctx, "cube_shadow_to_oct_fs", "src/shaders/cube_shadow_to_oct_frag.spv", vk::ShaderStageFlagBits::eFragment);

  PostProcessingPass<Nil, Nil> cubemap_to_oct{};
  cubemap_to_oct.init_attachment(vk::Format::eR32Sfloat);
  cubemap_to_oct.init(ds, 1, "cube_shadow_to_oct_fs");

  for (u32 i = 0; i < scene_lights.size(); i++) {
    auto layer_view = ds.storage.create_2Dlayer_view(ds.ctx, oct_shadows_img, vk::ImageAspectFlagBits::eColor, i);

    renderer.render(ds, cubemap, *this, scene_lights[i].position);
    scene_lights[i].shadow_layer = i;

    cubemap_to_oct.set_attachment(0, layer_view);
    cubemap_to_oct.set_image_sampler(0, view, sampler);
    cubemap_to_oct.set_render_area(1024, 1024);
    cubemap_to_oct.render_and_wait(ds);
  }

  cubemap_to_oct.release(ds);
  renderer.release(ds);

  ds.ctx.get_device().destroySampler(sampler);
}


void Scene::gen_textures(DriverState &ds) {
  const u32 mat_count = materials.size();
  scene_textures.materials.reserve(mat_count);

  vk::ImageSubresourceRange i_range {};
  i_range
    .setAspectMask(vk::ImageAspectFlagBits::eColor)
    .setBaseArrayLayer(0)
    .setBaseMipLevel(0)
    .setLayerCount(1)
    .setLevelCount(~0u);
  
  for (u32 i = 0; i < mat_count; i++) {
    SceneMaterial mat {.albedo_tex_id = -1, .mr_tex_id = -1};

    const auto &desc = materials[i];
    if (!desc.albedo_path.empty()) {
      auto img = ds.storage.load_image2D(ds.ctx, desc.albedo_path.c_str());
      auto view = ds.storage.create_image_view(ds.ctx, img, vk::ImageViewType::e2D, i_range);
      mat.albedo_tex_id = scene_textures.albedo_images.size();
      scene_textures.albedo_images.push_back(view);
    }

    if (!desc.mr_path.empty()) {
      auto img = ds.storage.load_image2D(ds.ctx, desc.mr_path.c_str());
      auto view = ds.storage.create_image_view(ds.ctx, img, vk::ImageViewType::e2D, i_range);
      mat.mr_tex_id = scene_textures.mr_images.size();
      scene_textures.mr_images.push_back(view);
    }

    ds.storage.collect_buffers();
    scene_textures.materials.push_back(mat);
  }
}