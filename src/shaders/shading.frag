#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "include/oct_coord.glsl"
#include "include/raytracing.glsl"
#include "include/trace_probe.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D albedo_tex;
layout (set = 0, binding = 1) uniform sampler2D normal_tex;
layout (set = 0, binding = 2) uniform sampler2D worldpos_tex;
layout (set = 0, binding = 3) uniform sampler2D depth_tex;

layout(set = 0, binding = 4) uniform samplerCube shadow_cube;


layout (push_constant) uniform PushData {
  vec3 camera_origin;
} pc;

void main() {
  vec3 world_dir = texture(worldpos_tex, uv).xyz;
  vec3 world_pos = world_dir + pc.camera_origin;
  vec3 norm = texture(normal_tex, uv).xyz;
  
  vec3 reflection_ray = normalize(reflect(world_dir, norm));
  
  vec3 ray_hit;
  uint probe_hit;
  float s = 1.f;
  vec3 start = world_pos;// + 1e-6 * norm;

  if (draw_probes(pc.camera_origin, world_pos)) {
    outColor = vec4(0.8, 0.8, 0.8, 1.f);
    return;
  }

  vec4 signalColor = vec4(0, 0, 0, 0);

  vec2 out_texc;
  int hit = -1;
  float trace_dist = 5.f;
  int result = trace(start, reflection_ray, trace_dist, out_texc, hit); 

  if (result == TRACE_RESULT_HIT) {
    signalColor = texture(probe_radiance, vec3(out_texc, hit));
  }
  
  outColor = 0.5 * texture(albedo_tex, uv) + 0.5 * signalColor;
}