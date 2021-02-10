#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput albedo_tex;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput normal_tex;
layout (input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput worldpos_tex;
layout (input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput depth_tex;

layout(set = 0, binding = 4) uniform samplerCube shadow;

layout (push_constant) uniform PushData {
  vec3 camera_origin;
} pc;

void main() {
  vec3 world_pos = subpassLoad(worldpos_tex).xyz + pc.camera_origin;
  vec3 norm = subpassLoad(normal_tex).xyz;
  
  vec3 light_pos = vec3(0, 4, 0);
  vec3 L = light_pos - world_pos;
  float dist = length(L);
  vec3 dir = L/dist;

  float light_dist = texture(shadow, -dir).r;


  float s = max(dot(dir, norm), 0);

  s *= (light_dist < dist - 0.001)? 0.f : 1.f;

  float atten = 1.f;
  outColor = atten * s * subpassLoad(albedo_tex);
}