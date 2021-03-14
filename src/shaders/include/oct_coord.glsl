#ifndef OCT_COORD_GLSL_INCLUDED
#define OCT_COORD_GLSL_INCLUDED

float sign_nz(in float k) {
  return (k >= 0.0) ? 1.0 : -1.0;
}

vec2 sign_nz(in vec2 v) {
  return vec2(sign_nz(v.x), sign_nz(v.y));
}

vec3 oct_to_sphere(vec2 uv) {
  uv = 2.f * (uv - vec2(0.5f, 0.5f));
  vec3 v = vec3(uv.x, uv.y, 1.0 - abs(uv.x) - abs(uv.y));
  if (v.z < 0.0) {
    v.xy = (1.0 - abs(v.yx)) * sign_nz(v.xy);
  }

  return normalize(v);
}

vec2 sphere_to_oct(in vec3 v) {
  float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
  vec2 result = v.xy * (1.0 / l1norm);
  if (v.z < 0.0) {
    result = (1.0 - abs(result.yx)) * sign_nz(result.xy);
  }
  return 0.5f * result + vec2(0.5f, 0.5f);
}

vec3 oct_decode(vec2 uv) {
  uv = 2.f * (uv - vec2(0.5f, 0.5f));
  vec3 v = vec3(uv.x, uv.y, 1.0 - abs(uv.x) - abs(uv.y));
  if (v.z < 0.0) {
    v.xy = (1.0 - abs(v.yx)) * sign_nz(v.xy);
  }

  return normalize(v);
}

vec2 oct_encode(in vec3 v) {
  float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
  vec2 result = v.xy * (1.0 / l1norm);
  if (v.z < 0.0) {
    result = (1.0 - abs(result.yx)) * sign_nz(result.xy);
  }
  return 0.5f * result + vec2(0.5f, 0.5f);
}


vec4 sample_octmap(in sampler2D octmap, in vec3 coord) {
  return texture(octmap, sphere_to_oct(coord));
}

#endif