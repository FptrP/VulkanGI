#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput albedo_tex;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput normal_tex;
layout (input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput depth_tex;

void main() {
  vec4 c = subpassLoad(albedo_tex);
  outColor = c;
}