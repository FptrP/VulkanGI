#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "include/oct_coord.glsl"

layout(location = 0) in vec2 uv;

layout (set = 0, binding = 0) uniform samplerCube dist_cm;
layout (set = 0, binding = 1) uniform samplerCube color_cm;
layout (set = 0, binding = 2) uniform samplerCube norm_cm;

layout(location = 0) out float out_dist;
layout(location = 1) out vec4 out_norm;
layout(location = 2) out vec4 out_color;

void main() {
  vec3 dir = oct_to_sphere(uv);
  vec3 center = oct_center(uv);

  vec3 r = dir * texture(dist_cm, dir).r;
  out_dist = dot(r, center);
  out_norm = texture(norm_cm, dir);
  out_color = texture(color_cm, dir);
}