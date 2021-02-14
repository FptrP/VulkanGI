#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 world_view;

layout(location = 0) out float dist;
layout(location = 1) out vec4 color;
layout(location = 2) out vec4 norm;

void main() {
  dist = length(world_view);
  color = vec4(0, 0, 0, 1);
  norm = vec4(0, 0, 0, 1);
}