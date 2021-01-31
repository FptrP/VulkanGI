#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler smp;
layout(set = 1, binding = 1) uniform texture2D textures[25];

layout (push_constant) uniform PushData {
  uint mat_id;
  uint tex_id;
} pc;

void main() {
  outColor = texture(sampler2D(textures[pc.tex_id], smp), uv);
}