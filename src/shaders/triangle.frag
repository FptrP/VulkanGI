#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 norm;
layout(location = 2) in vec3 world_pos;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNorm;
layout(location = 2) out vec4 outPos; 

layout(set = 1, binding = 0) uniform sampler smp;
layout(set = 1, binding = 1) uniform texture2D textures[25];
layout(set = 1, binding = 2) uniform texture2D material_tex[24];

layout (push_constant) uniform PushData {
  int mat_id;
  int tex_id;
  int mr_id;
} pc;

void main() {
  outColor = texture(sampler2D(textures[pc.tex_id], smp), uv);

  if (outColor.a == 0) {
    discard;
  }

  vec2 material = vec2(0.2, 0.8);

  if (pc.mr_id > 0) {
    material = texture(sampler2D(material_tex[pc.mr_id], smp), uv).rg; //r - meralness, g - roughness
  }

  outNorm = vec4(normalize(norm), material.r);
  outPos = vec4(world_pos, material.g);
}