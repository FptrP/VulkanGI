#version 450

const vec3 verts[] = vec3[](
  vec3(-1.0f,  1.0f, -1.0f),
  vec3(-1.0f, -1.0f, -1.0f),
  vec3(1.0f, -1.0f, -1.0f),
  vec3(1.0f, -1.0f, -1.0f),
  vec3(1.0f,  1.0f, -1.0f),
  vec3(-1.0f,  1.0f, -1.0f),

  vec3(-1.0f, -1.0f,  1.0f),
  vec3(-1.0f, -1.0f, -1.0f),
  vec3(-1.0f,  1.0f, -1.0f),
  vec3(-1.0f,  1.0f, -1.0f),
  vec3(-1.0f,  1.0f,  1.0f),
  vec3(-1.0f, -1.0f,  1.0f),

  vec3(1.0f, -1.0f, -1.0f),
  vec3(1.0f, -1.0f,  1.0f),
  vec3(1.0f,  1.0f,  1.0f),
  vec3(1.0f,  1.0f,  1.0f),
  vec3(1.0f,  1.0f, -1.0f),
  vec3(1.0f, -1.0f, -1.0f),

  vec3(-1.0f, -1.0f,  1.0f),
  vec3(-1.0f,  1.0f,  1.0f),
  vec3(1.0f,  1.0f,  1.0f),
  vec3(1.0f,  1.0f,  1.0f),
  vec3(1.0f, -1.0f,  1.0f),
  vec3(-1.0f, -1.0f,  1.0f),

  vec3(-1.0f,  1.0f, -1.0f),
  vec3(1.0f,  1.0f, -1.0f),
  vec3(1.0f,  1.0f,  1.0f),
  vec3(1.0f,  1.0f,  1.0f),
  vec3(-1.0f,  1.0f,  1.0f),
  vec3(-1.0f,  1.0f, -1.0f),

  vec3(-1.0f, -1.0f, -1.0f),
  vec3(-1.0f, -1.0f,  1.0f),
  vec3(1.0f, -1.0f, -1.0f),
  vec3(1.0f, -1.0f, -1.0f),
  vec3(-1.0f, -1.0f,  1.0f),
  vec3(1.0f, -1.0f,  1.0f)
);

layout (location = 0) out vec3 view;

layout (set = 0, binding = 0) uniform MVP {
  mat4 mvp;
};

void main() {
  gl_Position = mvp * vec4(verts[gl_VertexIndex], 1.f);
  view = verts[gl_VertexIndex];
}