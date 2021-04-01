#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "include/shadows.glsl"

layout(location = 0) in vec3 world_view;
layout(location = 1) in vec3 world_normal;
layout(location = 2) in vec2 uv;
layout (location = 3) in vec3 world_pos;

layout(location = 0) out float dist;
layout(location = 1) out vec4 color;
layout(location = 2) out vec4 norm;

layout(set = 0, binding = 2) uniform texture2D textures[25];
layout(set = 0, binding = 3) uniform sampler tex_smp;

#define MAX_LIGHTS 4

layout(set = 0, binding = 4) uniform LightData {
  vec4 lights_count;
  vec4 position[MAX_LIGHTS];
  vec4 radiance[MAX_LIGHTS];
} lights;

layout (set = 0, binding = 5) uniform sampler2DArray shadows;

layout (push_constant) uniform PushData {
  uint mat_id;
  uint albedo_id;
} pc;

void main() {
  dist = length(world_view);
  vec4 albedo = texture(sampler2D(textures[pc.albedo_id], tex_smp), uv);
  
  if (albedo.a == 0) {
    discard;
  }
  
  uint lights_count = uint(min(lights.lights_count.x, MAX_LIGHTS));
  vec3 irradiance = vec3(0);
  
  const float PI = 3.14159265359;
  float visibility = 0.f;
  for (uint i = 0; i < lights_count; i++) {
    vec3 N = normalize(world_normal);
    vec3 L = lights.position[i].xyz - world_pos;
    float dist2 = dot(L, L);
    float atten = 1.f/dist2;

    irradiance += albedo.xyz/PI * lights.radiance[i].xyz * atten * max(dot(N, L), 0.f);

    visibility += pcf_octmap(shadows, float(i), -L, sqrt(dist2));

  }

  visibility /= float(lights_count);

  color.xyz = visibility * irradiance;
  norm = vec4(world_normal, 1);
}