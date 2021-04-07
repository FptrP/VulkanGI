#version 450 core

#include "include/real_sh.glsl"
#include "include/oct_coord.glsl"

layout (location = 0) in vec3 view;
layout (location = 0) out vec4 color;

layout (std430, set = 0, binding = 1) readonly buffer SHBuffer {
  ShProbe probes[];
};

layout (set = 0, binding = 2) uniform sampler2DArray tex_probes;

layout (push_constant) uniform Settings {
  uint flags;
  uint probe_id;
};

const uint DRAW_SH  = 0;
const uint DRAW_TEX = 1;
const uint DRAW_DIFF = 2;

void main() {
  vec3 w = normalize(view);
  //w.z = -w.z;
  const float NORM = 20.f;
  if (flags == DRAW_SH) {
    float sh_distance = sample_sh36(probes[probe_id], w);
    color = vec4(sh_distance, sh_distance, sh_distance, 0)/NORM;
    return;
  }

  if (flags == DRAW_TEX) {
    float dist = 0.f;
    vec2 uv = oct_encode(w);
    dist = texture(tex_probes, vec3(uv, probe_id)).r;
    color = vec4(dist, dist, dist, 0)/NORM;
    return;
  }

  if (flags == DRAW_DIFF) {
    float sh_distance = sample_sh36(probes[probe_id], w);
    float dist = 0.f;
    vec2 uv = oct_encode(w);
    dist = texture(tex_probes, vec3(uv, probe_id)).r;
    
    float dif = abs(sh_distance - dist);
    float overflow = (dif > 0.5f)? 1.f : 0.f;
    if (sh_distance > dist) {
      color = vec4(dif, 0.f, 0.f, 0.f);
    } else {
      color = vec4(0, dif, 0.f, 0.f);
    }
    return;
  }

  color = vec4(0, 0, 0, 0);
}

