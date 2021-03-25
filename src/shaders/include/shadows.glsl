

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

float pcf_octmap(in sampler2D shadow_tex, vec3 dir, float dist) {
  const float bias = 0.05; 
  const float samples = 4.0;
  const float offset  = 0.01;

  float s = 0.f;

  for(float x = -offset; x < offset; x += offset / (samples * 0.5)) {
    for(float y = -offset; y < offset; y += offset / (samples * 0.5)) {
      for(float z = -offset; z < offset; z += offset / (samples * 0.5)) {
        float light_dist = texture(shadow_tex, sphere_to_oct(dir + vec3(x, y, z))).r; 
        s += (light_dist < dist - bias)? 0.f : 1.f;
      }
    }
  }
  return s/(samples * samples * samples);
}

float pcf_octmap(in sampler2DArray shadow_tex, float layer, vec3 dir, float dist) {
  const float bias = 0.05; 
  const float samples = 1.0;
  const float offset  = 0.005;

  float s = 0.f;

  for(float x = -offset; x < offset; x += offset / (samples * 0.5)) {
    for(float y = -offset; y < offset; y += offset / (samples * 0.5)) {
      for(float z = -offset; z < offset; z += offset / (samples * 0.5)) {
        float light_dist = texture(shadow_tex, vec3(sphere_to_oct(dir + vec3(x, y, z)), layer)).r; 
        s += (light_dist < dist - bias)? 0.f : 1.f;
      }
    }
  }
  return s/(samples * samples * samples);
}