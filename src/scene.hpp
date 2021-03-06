#ifndef SCENE_HPP_INCLUDED
#define SCENE_HPP_INCLUDED

#include "drv/common.hpp"
#include "driverstate.hpp"

#include <assimp/Importer.hpp>      
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>

#include <glm/glm.hpp>

struct SceneObject {
  u32 vertex_offset;
  u32 index_offset;
  u32 index_count;
  u32 material_index;
  u32 matrix_index;
};

struct SceneMesh {
  u32 vertex_offset;
  u32 index_offset;
  u32 index_count;
  u32 material;
};

struct SceneVertex {
  glm::vec3 pos;
  glm::vec3 norm;
  glm::vec2 uv;
};

struct SceneMaterialDesc {
  std::string albedo_path;
  std::string mr_path;
};

struct SceneLight {
  glm::vec3 position {};
  glm::vec3 color {};
  u32 shadow_layer;
};

struct SceneMaterial {
  int albedo_tex_id;
  int mr_tex_id;
};

struct SceneTextures {
  std::vector<SceneMaterial> materials;
  std::vector<drv::ImageViewID> albedo_images;
  std::vector<drv::ImageViewID> mr_images;
};

struct Scene {
  void load(const std::string &path, const std::string &folder);
  void gen_buffers(DriverState &ds);

  const std::vector<SceneObject>& get_objects() const { return objects; }

  const drv::BufferID &get_matrix_buff() const { return matrix_buff; }
  const drv::BufferID &get_index_buff() const { return index_buff; }
  const drv::BufferID &get_verts_buff() const { return verts_buff; }

  const std::vector<SceneMaterialDesc> &get_material_desc() const { return materials; }

  void add_light(glm::vec3 pos, glm::vec3 color) {
    SceneLight light {};
    light.position = pos;
    light.color = color;
    scene_lights.push_back(light);
  }

  void gen_shadows(DriverState &ds);
  std::vector<SceneLight> &get_lights() { return scene_lights; }

  void gen_textures(DriverState &ds);
  SceneTextures &get_materials() { return scene_textures; }

  drv::ImageViewID get_shadows_array() { return oct_shadows_array; }

private:
  void process_meshes(const aiScene *scene);
  void process_materials(const aiScene *scene);
  void process_objects(const aiNode *node, glm::mat4 transform);


  std::vector<glm::mat4> matrices;
  std::vector<SceneVertex> verts;
  std::vector<u32> indexes;
  std::vector<SceneObject> objects;
  std::vector<SceneMesh> meshes;
  std::vector<SceneMaterialDesc> materials;
  std::vector<SceneLight> scene_lights;
  
  SceneTextures scene_textures;

  drv::BufferID verts_buff, index_buff, matrix_buff;
  
  drv::ImageViewID oct_shadows_array;

  std::string model_path;
};

#endif