#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 uv;

layout (set = 0, binding = 0) uniform samplerCube cubemap;

layout (location = 0) out float color;

vec3 oct_to_sphere(vec2 uv);

void main() {
  color = texture(cubemap, oct_to_sphere(uv)).r;
}

float signNotZero(in float k) {
  return (k >= 0.0) ? 1.0 : -1.0;
}


vec2 signNotZero(in vec2 v) {
  return vec2(signNotZero(v.x), signNotZero(v.y));
}


vec3 oct_to_sphere(vec2 uv) {
  uv = 2.f * (uv - vec2(0.5f, 0.5f));
  vec3 v = vec3(uv.x, uv.y, 1.0 - abs(uv.x) - abs(uv.y));
  if (v.z < 0.0) {
    v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
  }

  return normalize(v);
}

vec2 sphere_to_oct(in vec3 v) {
  float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
  vec2 result = v.xy * (1.0 / l1norm);
  if (v.z < 0.0) {
    result = (1.0 - abs(result.yx)) * signNotZero(result.xy);
  }
  return 0.5f * result + vec2(0.5f, 0.5f);
}
