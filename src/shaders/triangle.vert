#version 450

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_norm;
layout (location = 2) in vec2 in_uv;

//layout (location = 0) out vec3 norm;
layout (location = 0) out vec2 uv;

layout (set = 0, binding = 0) uniform VertexData {
  mat4 camera;
};

layout (set = 0, binding = 1) readonly buffer Matrices {
  mat4 matrices[];
};

layout (push_constant) uniform PushData {
  uint mat_id;
  uint tex_id;
};

void main() {
  gl_Position = camera * matrices[mat_id] * vec4(in_pos, 1.0);
  uv = in_uv;
}