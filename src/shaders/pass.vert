#version 450

const vec2 in_pos[] = vec2[](
  vec2(-1, -1),
  vec2(-1, 3),
  vec2(3, -1)
);

const vec2 in_uv[] = vec2[](
  vec2(0, 0),
  vec2(0, 2),
  vec2(2, 0)
);

layout(location = 0) out vec2 uv;

void main() {
  gl_Position = vec4(in_pos[gl_VertexIndex], 0, 1);
  uv = in_uv[gl_VertexIndex];
}