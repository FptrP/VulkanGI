#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 world_view;
layout(location = 1) in vec3 world_normal;
layout(location = 2) in vec2 uv;

layout(location = 0) out float dist;
layout(location = 1) out vec4 color;
layout(location = 2) out vec4 norm;

layout(set = 0, binding = 2) uniform texture2D textures[25];
layout(set = 0, binding = 3) uniform sampler tex_smp;

layout (push_constant) uniform PushData {
  uint mat_id;
  uint albedo_id;
} pc;

void main() {
  dist = length(world_view);
  color = texture(sampler2D(textures[pc.albedo_id], tex_smp), uv);
  if (color.a == 0) {
    discard;
  }
  norm = vec4(world_normal, 1);
}