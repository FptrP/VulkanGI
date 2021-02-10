#version 450

layout (location = 0) in vec3 in_pos;

layout (location = 0) out vec3 world_view;

layout (set = 0, binding = 0) uniform VertexData {
  mat4 camera_proj;
  vec4 camera_origin;
};

layout (set = 0, binding = 1) readonly buffer Matrices {
  mat4 matrices[];
};

layout (push_constant) uniform PushData {
  uint mat_id;
};

void main() {
  vec4 w = matrices[mat_id] * vec4(in_pos, 1.0);
  world_view = camera_origin.xyz - w.xyz;
  gl_Position = camera_proj * w; 
}