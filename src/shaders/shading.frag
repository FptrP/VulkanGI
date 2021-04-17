#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "include/oct_coord.glsl"
#include "include/raytracing.glsl"
#include "include/brdf.glsl"
#include "include/trace_probe.glsl"
#include "include/shadows.glsl"
#include "include/real_sh.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D albedo_tex;
layout (set = 0, binding = 1) uniform sampler2D normal_tex;
layout (set = 0, binding = 2) uniform sampler2D worldpos_tex;
layout (set = 0, binding = 3) uniform sampler2D depth_tex;

layout(set = 0, binding = 4) uniform sampler2DArray oct_shadows;

#define MAX_LIGHTS 4

layout(set = 0, binding = 5) uniform LightSourceInfo {
  vec4 lights_count;
  vec4 position[MAX_LIGHTS];
  vec4 radiance[MAX_LIGHTS];
} lights;

layout(set = 0, binding = 6) readonly buffer Sh {
  ShProbe sh_probes[];
};

layout (push_constant) uniform PushData {
  vec3 camera_origin;
} pc;


bool trace_sh(in vec3 origin, in vec3 dir, uint probe_id, inout float tmin, inout float tmax);

void main() {
  vec3 world_dir = texture(worldpos_tex, uv).xyz;
  vec3 world_pos = world_dir + pc.camera_origin;
  vec3 norm = texture(normal_tex, uv).xyz;
  vec3 albedo = texture(albedo_tex, uv).rgb;

  //albedo = pow(albedo, vec3(2.2));
  //albedo = albedo/(1 - albedo);

  float metalic = texture(normal_tex, uv).w;
  float roughness = texture(worldpos_tex, uv).w;

  
  vec3 ray_hit;
  uint probe_hit;
  float s = 1.f;

  if (draw_probes(pc.camera_origin, world_pos)) {
    outColor = vec4(0.8, 0.8, 0.8, 1.f);
    return;
  }

  vec4 signalColor = vec4(0, 0, 0, 0);

  float shadow = 0.f;
  float lights_count = min(lights.lights_count.x, MAX_LIGHTS);

  vec3 irradiance = vec3(0);

  for (float i = 0; i < lights_count; i += 1.f) {
    vec3 light_dir = lights.position[int(i)].xyz - world_pos;

    irradiance += calc_color(
      pc.camera_origin, 
      world_pos, norm, 
      albedo, 
      lights.position[int(i)].xyz,
      lights.radiance[int(i)].xyz,
      metalic,
      roughness);
    
    shadow += pcf_octmap(oct_shadows, i, -light_dir, length(light_dir));
  }
  
  shadow /= lights_count;
  vec3 reflected = vec3(0);
  if (true) {
    vec2 out_texc;
    int hit = -1;
    vec3 start = world_pos + 1e-6 * norm;
    vec3 reflection_ray = normalize(reflect(world_dir, norm));
    float trace_dist = 5.f;
    int result = trace(start, reflection_ray, trace_dist, out_texc, hit); 

    if (result == TRACE_RESULT_HIT) {
      vec3 reflection = texture(probe_radiance, vec3(out_texc, hit)).rgb;
      vec3 hitpos = world_pos + reflection_ray * trace_dist;
      
      vec3 F = calcFresnel(pc.camera_origin, world_pos, norm, hitpos, albedo, metalic, roughness);
      reflected += 0.5 * F * reflection;
    }
  }

  outColor = 3 * vec4(reflected, 0) + shadow * vec4(irradiance, 0.f) + 0.1 * vec4(albedo * computePrefilteredIrradiance(world_pos, norm), 0.f);
}

bool trace_sh(in vec3 origin, in vec3 dir, uint probe_id, inout float tmin, inout float tmax) {
  return true;
}