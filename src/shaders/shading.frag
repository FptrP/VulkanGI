#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D albedo_tex;
layout (set = 0, binding = 1) uniform sampler2D normal_tex;
layout (set = 0, binding = 2) uniform sampler2D worldpos_tex;
layout (set = 0, binding = 3) uniform sampler2D depth_tex;

layout(set = 0, binding = 4) uniform samplerCube shadow;

layout (push_constant) uniform PushData {
  vec3 camera_origin;
} pc;

float pcf_cubemap(in samplerCube shadow_tex, vec3 dir, float dist);

void main() {
  vec3 world_pos = texture(worldpos_tex, uv).xyz + pc.camera_origin;
  vec3 norm = texture(normal_tex, uv).xyz;
  
  vec3 light_pos = vec3(0, 4, 0);
  vec3 L = light_pos - world_pos;
  float dist = length(L);
  vec3 dir = L/dist;

  float s = max(dot(dir, norm), 0);

  s *= pcf_cubemap(shadow, -dir, dist);

  float atten = 1.f;
  outColor = atten * s * texture(albedo_tex, uv);
}

float pcf_cubemap(in samplerCube shadow_tex, vec3 dir, float dist) {
  float s  = 0.0;
  const float bias    = 0.05; 
  const float samples = 8.0;
  const float offset  = 0.01;
  
  for(float x = -offset; x < offset; x += offset / (samples * 0.5)) {
    for(float y = -offset; y < offset; y += offset / (samples * 0.5)) {
      for(float z = -offset; z < offset; z += offset / (samples * 0.5)) {
        float light_dist = texture(shadow_tex, dir + vec3(x, y, z)).r; 
        s += (light_dist < dist - bias)? 0.f : 1.f;
      }
    }
  }
  
  return s/(samples * samples * samples);
}