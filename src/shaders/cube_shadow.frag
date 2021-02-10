#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 world_view;

layout(location = 0) out float dist;

void main() {
  dist = length(world_view);
}