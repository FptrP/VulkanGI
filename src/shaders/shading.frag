#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "include/oct_coord.glsl"
#include "include/raytracing.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D albedo_tex;
layout (set = 0, binding = 1) uniform sampler2D normal_tex;
layout (set = 0, binding = 2) uniform sampler2D worldpos_tex;
layout (set = 0, binding = 3) uniform sampler2D depth_tex;

layout(set = 0, binding = 4) uniform samplerCube shadow_cube;

#define MAX_PROBES 27

#define RAY_MISS 0
#define RAY_HIT 1
#define RAY_UNKNOWN 2
#define RAY_TRACE_ERR 3

layout(set = 1, binding = 0) uniform LightFieldData {
  vec4 dim;
  vec4 bmin;
  vec4 bmax;
  vec4 positions[MAX_PROBES];
} lightfield;

layout(set = 1, binding = 1) uniform sampler2DArray probe_dist; 

layout (push_constant) uniform PushData {
  vec3 camera_origin;
} pc;

float pcf_cubemap(in samplerCube shadow_tex, vec3 dir, float dist);
float pcf_octmap(in sampler2D shadow_tex, vec3 dir, float dist);


uint probe_to_index(uvec3 probe);
uint next_probe(uvec3 probe, uint offs);
uvec3 closest_probe(vec3 point);

//uint trace_ray(vec3 origin, vec3 dir, float min_t, float max_t, out vec3 out_ray);
uint trace_probe(uint probe_id, vec3 start, vec3 end, out vec3 out_r);
uint trace_ray(vec3 origin, vec3 dir, float min_t, float max_t, out vec3 out_ray);

bool draw_probes(vec3 camera, vec3 world_pos);

void main() {
  vec3 world_pos = texture(worldpos_tex, uv).xyz + pc.camera_origin;
  vec3 norm = texture(normal_tex, uv).xyz;
  
  vec3 light_pos = vec3(0, 4, 0);
  vec3 L = light_pos - world_pos;
  
  vec3 ray_hit;
  float s = 1.f;
  vec3 start = world_pos + 0.01 * norm;

  if (draw_probes(pc.camera_origin, world_pos)) {
    outColor = vec4(0.8, 0.8, 0.8, 1.f);
    return;
  }

  uint trace_res;
  for (uint i = 0; i < 3; i++) {
    trace_res = trace_ray(start, L, 0.003f, 1.f, ray_hit); 
    if (trace_res == RAY_HIT) {
      s = 0.f;
      break;
    }
    if (trace_res == RAY_MISS) {
      break;
    }

    start = ray_hit;
    L = light_pos - start;
  }

  if (trace_res == RAY_UNKNOWN) {
    outColor = vec4(1, 0, 0, 0);
    return;
  }
  
  //float dist = length(L);
  //vec3 dir = L/dist;

  //float s = max(dot(dir, norm), 0);

  //s *= pcf_cubemap(shadow_cube, -dir, dist);

  float atten = 1.f;
  outColor = atten * s * texture(albedo_tex, uv);
  //float c = texture(sampler2D(probe_dist[3], probe_sampler), uv).r/10.f;
  //outColor = abs(vec4(lightfield.positions[closest_probe(world_pos)].xyz, 0.f);
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

uint probe_to_index(uvec3 probe) {
  uvec3 udim = uvec3(lightfield.dim.xyz);
  return clamp(probe.z + probe.y * udim.z + probe.x * udim.y * udim.z, 0, udim.x * udim.y * udim.z - 1);
}

uint next_probe(uvec3 probe, uint offs) {
  uvec3 d = uvec3(offs & 1, (offs >> 1) & 1, (offs >> 2) & 1);
  return probe_to_index(probe + d);
}

uvec3 closest_probe(vec3 point) {
  point = clamp(point, lightfield.bmin.xyz, lightfield.bmax.xyz);
  vec3 bbox_size = (lightfield.bmax.xyz - lightfield.bmin.xyz);
  vec3 cell_size = bbox_size/lightfield.dim.xyz;  
  uvec3 udim = uvec3(lightfield.dim.xyz);
  
  uvec3 cell = uvec3(floor((point - lightfield.bmin.xyz)/cell_size));
  
  return clamp(cell, uvec3(0, 0, 0), uvec3(udim.x - 1, udim.y - 1, udim.z - 1));
}

void min_swap(inout float a, inout float b) {
  float temp = min(a, b);
  b = max(a, b);
  a = temp;
}

#define TRACE_ERR(x) if (isnan((x)) || isinf((x))) {return RAY_TRACE_ERR;}

uint trace_segment(uint probe_id, vec3 start, vec3 end, out vec3 out_r) {
  const float TEX_SIZE = 1024;

  vec2 uv_start = sphere_to_oct(start);
  vec2 uv_end = sphere_to_oct(end);

  vec2 pixel_delta = abs((uv_end - uv_start) * TEX_SIZE);
  float steps = 30.f;

  if (steps < 1.f) {
    return RAY_MISS;
  }

  vec3 dray = (end - start);
  
  for (float s = 0; s < steps; s += 1.f) {
    vec3 r = start + s * dray;
    vec2 uv = sphere_to_oct(r);

    float geom_dist = texelFetch(probe_dist, ivec3(uv * TEX_SIZE, probe_id), 0).r;
    float ray_dist = length(r);

    if (ray_dist > geom_dist + 0.15) {
      out_r = r;
      return RAY_UNKNOWN;
    }

    if (ray_dist > geom_dist) {
      out_r = geom_dist * oct_to_sphere(uv);
      return RAY_HIT;
    }
  }

  return RAY_MISS;
}

uint trace_probe(uint probe_id, vec3 start, vec3 end, out vec3 out_r) {
  vec3 probe_pos = lightfield.positions[probe_id].xyz;
  start -= probe_pos;
  end -= probe_pos;

  vec3 ray_delta = end - start;
  vec3 borders = -start/ray_delta;

  min_swap(borders.x, borders.y);
  min_swap(borders.y, borders.z);
  min_swap(borders.x, borders.y);

  float segments[5];
  segments[0] = 0.01;
  segments[1] = clamp(borders.x, 0, 1);
  segments[2] = clamp(borders.y, 0, 1);
  segments[3] = clamp(borders.z, 0, 1);
  segments[4] = 1;
  
  vec3 r;

  for (uint i = 1; i < 5; i++) {
    vec3 seg_start = start + ray_delta * segments[i-1];
    vec3 seg_end = start + ray_delta * segments[i];
    uint res = trace_segment(probe_id, seg_start, seg_end, r);

    if (res == RAY_HIT || res == RAY_UNKNOWN || res == RAY_TRACE_ERR) {
      out_r = r + probe_pos;
      return res;
    }
  }

  return RAY_MISS;
} 
/*
uint trace_ray(vec3 origin, vec3 dir, float min_t, float max_t, out vec3 out_ray) {
  float dir_len = length(dir);

  dir = normalize(dir);
  min_t *= dir_len;
  max_t *= dir_len;

  uvec3 probe_pos = closest_probe(origin);
  float t;
  uint res;

  for (uint probe_offs = 0; probe_offs < 1; probe_offs++) {
    uint probe_id = next_probe(probe_pos, probe_offs);
    res = trace_probe(probe_id, origin, dir, min_t, max_t, t);

    if (res == RAY_HIT) {
      out_ray = origin + t * dir;
      return RAY_HIT;
    }

    if (res == RAY_MISS) {
      return RAY_MISS;
    }

    if (res == RAY_TRACE_ERR) {
      return res;
    }
    //RAY_UNCKOWN
    min_t = t;
    if (min_t > max_t) {
      return RAY_UNKNOWN;
    }
  }

  return res;
}*/


float ray_intersection_dist(vec3 origin, vec3 ray_dir, vec3 v) {
  float numer;
  float denom = v.y * ray_dir.z - v.z * ray_dir.y;

  if (abs(denom) > 0.1) {
    numer = origin.y * ray_dir.z - origin.z * ray_dir.y;
  } else {
    numer = origin.x * ray_dir.y - origin.y * ray_dir.x;
    denom = v.x * ray_dir.y - v.y * ray_dir.x;
  }

  return numer / denom;
}

uint closest_probe_slow(vec3 point) {
  uint min_ix = 0;
  float min_dist = length(point - lightfield.positions[min_ix].xyz);
  for (uint i = 1; i < MAX_PROBES; i++) {
    float dist = length(point - lightfield.positions[i].xyz);
    if (dist < min_dist) {
      min_dist = dist;
      min_ix = i;
    }
  }
  return min_ix;
}

float ray_vec_intersection(vec3 o, vec3 d, vec3 v) {
  vec3 numer;
  vec3 denom;

  numer.x = o.x * d.y - o.y * d.x;
  denom.x = v.x * d.y - v.y * d.x;
  numer.y = o.x * d.z - o.z * d.x;
  denom.y = v.x * d.z - v.z * d.x;
  numer.z = o.y * d.z - o.z * d.y;
  denom.z = v.y * d.z - v.z * d.y;

  const float EPS = 0.001;
  if (abs(denom.x) > EPS) return numer.x/denom.x;
  if (abs(denom.y) > EPS) return numer.y/denom.y;
  if (abs(denom.z) > EPS) return numer.z/denom.z;

  return -1;
}

uint trace_probe(uint probe_id, vec3 origin, vec3 dir, float min_t, float max_t, out vec3 out_ray) {
  vec3 start = origin + dir * min_t;
  //uint probe_id = probe_to_index(closest_probe(origin));

  vec3 probe_pos = lightfield.positions[probe_id].xyz;
  start = start - probe_pos;

  vec3 origin_probe = origin - probe_pos;

  vec3 borders = -start/dir;

  min_swap(borders.x, borders.y);
  min_swap(borders.y, borders.z);
  min_swap(borders.x, borders.y);

  float segments[5];
  segments[0] = min_t;
  segments[1] = clamp(borders.x, min_t, max_t);
  segments[2] = clamp(borders.y, min_t, max_t);
  segments[3] = clamp(borders.z, min_t, max_t);
  segments[4] = max_t;

  const vec2 tex_size = vec2(1024, 1024);

  for (uint i = 1; i < 5; i++) {
    //vec3 start = origin + segments[i - 1] * dir - probe_pos;
    vec3 end = origin + segments[i] * dir - probe_pos;
    
    vec2 start_oct = sphere_to_oct(start);
    vec2 end_oct = sphere_to_oct(end);
    
    vec2 pixel_dist = abs(end_oct - start_oct) * tex_size;
    float steps = 2 * max(pixel_dist.x, pixel_dist.y);

    if (steps < 1.f) continue;

    vec2 duv = (end_oct - start_oct)/steps;


    for (float s = 0; s < steps; s++) {
      vec2 uv = start_oct + s * duv;
      vec3 oct_to_point = normalize(oct_to_sphere(uv));
      float dist = texelFetch(probe_dist, ivec3(uv * tex_size, probe_id), 0).r;
      float ray_dist = ray_vec_intersection(origin_probe, dir, oct_to_point);
      vec3 r = oct_to_point * ray_dist;

      if (ray_dist > dist + 0.05) {
        out_ray  = r;
        return RAY_UNKNOWN;
      }

      if (ray_dist >= dist) {
        out_ray  = oct_to_point * ray_dist + probe_pos;
        return RAY_HIT;
      }
    }

    start = end;
  }

  return RAY_MISS;
}

uint trace_ray(vec3 origin, vec3 dir, float min_t, float max_t, out vec3 out_ray) {
  uint res = RAY_UNKNOWN;
  vec3 ray;
  uvec3 start_probe = closest_probe(origin);

  for (uint i = 0; i < 8; i++) {
    uint id = next_probe(start_probe, i);
    res = trace_probe(id, origin, dir, min_t, max_t, ray);
    
    if (res == RAY_HIT || res == RAY_MISS) {
      out_ray = ray;
      return res;
    }

    vec3 end = origin + dir * max_t;
    vec3 origin = ray;
    min_t = 0.0001;
    max_t = 1;
    dir = end - origin;  
  }


  return res;
}

bool draw_probes(vec3 camera, vec3 world_pos) {
  
  float pixel_dist = length(world_pos - camera);
  vec3 dir = (world_pos - camera)/pixel_dist;

  const float R = 0.05f;
  for (uint i = 0; i < MAX_PROBES; i++) {
    float t;
    if (ray_sphere_intersection(camera, dir, lightfield.positions[i].xyz, R, t)) {
      if (t < pixel_dist) {
        return true;
      }
    }
  }
  return false;
}