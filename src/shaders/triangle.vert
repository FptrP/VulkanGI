#version 450

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_norm;
layout (location = 2) in vec2 in_uv;

//layout (location = 0) out vec3 norm;
layout (location = 0) out vec2 uv;
layout (location = 1) out vec3 norm;
layout (location = 2) out vec3 world_pos;

layout (set = 0, binding = 0) uniform VertexData {
  mat4 camera;
  mat4 inv_camera;
  mat4 project;
  vec4 camera_origin;
};

layout (set = 0, binding = 1) readonly buffer Matrices {
  mat4 matrices[];
};

layout (push_constant) uniform PushData {
  int mat_id;
  int tex_id;
  int mr_id;
};

void main() {
  vec4 w = matrices[mat_id] * vec4(in_pos, 1.0);
  
  gl_Position = project * camera * w;
  
  uv = in_uv;
  norm = in_norm;
  world_pos = w.xyz - camera_origin.xyz;
}