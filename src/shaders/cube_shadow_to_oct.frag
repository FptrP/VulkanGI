#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "include/oct_coord.glsl"

layout(location = 0) in vec2 uv;

layout (set = 0, binding = 0) uniform samplerCube shadow;

layout(location = 0) out float out_shadow;

void main() {
  vec3 dir = oct_decode(uv);
  out_shadow = texture(shadow, dir).r;
}