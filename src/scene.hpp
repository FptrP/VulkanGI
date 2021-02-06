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

struct SceneMaterial {
  std::string albedo_path;
  std::string mr_path;
};

struct Scene {
  void load(const std::string &path, const std::string &folder);
  void gen_buffers(DriverState &ds);

  const std::vector<SceneObject>& get_objects() const { return objects; }

  drv::BufferID &get_matrix_buff() { return matrix_buff; }
  drv::BufferID &get_index_buff() { return index_buff; }
  drv::BufferID &get_verts_buff() { return verts_buff; }

  const std::vector<SceneMaterial> &get_materials() const { return materials; }

private:
  void process_meshes(const aiScene *scene);
  void process_materials(const aiScene *scene);
  void process_objects(const aiNode *node, glm::mat4 transform);


  std::vector<glm::mat4> matrices;
  std::vector<SceneVertex> verts;
  std::vector<u32> indexes;
  std::vector<SceneObject> objects;
  std::vector<SceneMesh> meshes;
  std::vector<SceneMaterial> materials;

  drv::BufferID verts_buff, index_buff, matrix_buff;

  std::string model_path;
};

#endif