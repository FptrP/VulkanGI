#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "include/oct_coord.glsl"
#include "include/raytracing.glsl"
#include "include/trace_probe.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D albedo_tex;
layout (set = 0, binding = 1) uniform sampler2D normal_tex;
layout (set = 0, binding = 2) uniform sampler2D worldpos_tex;
layout (set = 0, binding = 3) uniform sampler2D depth_tex;

layout(set = 0, binding = 4) uniform samplerCube shadow_cube;
/*
#define MAX_PROBES 256

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
layout(set = 1, binding = 2) uniform sampler2DArray probe_norm;*/

layout (push_constant) uniform PushData {
  vec3 camera_origin;
} pc;
/*
uint probe_to_index(uvec3 probe);
uint next_probe(uvec3 probe, uint offs);
uvec3 closest_probe(vec3 point);

uint trace_probe(uint probe_id, vec3 start, vec3 end, out vec3 out_r);
uint trace_ray(vec3 origin, vec3 dir, float min_t, float max_t, out vec3 out_ray, out uint out_probe);

bool draw_probes(vec3 camera, vec3 world_pos);*/

void main() {
  vec3 world_pos = texture(worldpos_tex, uv).xyz + pc.camera_origin;
  vec3 norm = texture(normal_tex, uv).xyz;
  
  vec3 light_pos = vec3(-0.180025, 2.29345, 0.00821752);
  vec3 L = light_pos - world_pos;
  
  vec3 ray_hit;
  uint probe_hit;
  float s = 1.f;
  vec3 start = world_pos + 0.005 * norm;

  if (draw_probes(pc.camera_origin, world_pos)) {
    outColor = vec4(0.8, 0.8, 0.8, 1.f);
    return;
  }
  vec4 signalColor = vec4(0, 0, 0, 0);

  float L_dist = length(L);
  vec2 out_texc;
  int hit = -1;
  bool ok = trace(start, L/L_dist, L_dist, out_texc, hit); 

  if (ok) {
    s = 0.f;
  }
  

  float atten = 1.f;
  outColor = atten * s * texture(albedo_tex, uv) + signalColor;
}
/*
uint probe_to_index(uvec3 probe) {
  uvec3 udim = uvec3(lightfield.dim.xyz);
  return clamp(probe.z + probe.y * udim.z + probe.x * udim.y * udim.z, 0, udim.x * udim.y * udim.z - 1);
}

uint next_probe(uvec3 probe, uint offs) {
  ivec3 dim = ivec3(lightfield.dim.xyz);
  ivec3 d = ivec3(offs & 1, (offs >> 1) & 1, (offs >> 2) & 1);
  ivec3 p = ivec3(probe);
  
  return probe_to_index(uvec3(min(max(p + d, 0), dim - 1)));
}

uvec3 closest_probe(vec3 point) {
  point = clamp(point, lightfield.bmin.xyz, lightfield.bmax.xyz);
  vec3 bbox_size = (lightfield.bmax.xyz - lightfield.bmin.xyz);
  vec3 cell_size = bbox_size/lightfield.dim.xyz;  
  uvec3 udim = uvec3(lightfield.dim.xyz);
  
  uvec3 cell = uvec3(floor((point - lightfield.bmin.xyz - 0.5 * cell_size)/cell_size));
  cell = clamp(cell, uvec3(0, 0, 0), uvec3(udim.x - 1, udim.y - 1, udim.z - 1));
  return cell; 
}

void min_swap(inout float a, inout float b) {
  float temp = min(a, b);
  b = max(a, b);
  a = temp;
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

uint trace_segment(uint pid, vec3 origin, vec3 dir, vec2 uv_start, vec2 uv_end, out float out_t) {
  const float TEX_SIZE = 1024;
  
  vec2 p_start = floor(uv_start * TEX_SIZE);
  vec2 p_end = floor(uv_end * TEX_SIZE);

  vec2 uv_delta = uv_end - uv_start;
  vec2 pixel_delta = p_end - p_start;

  float steps = max(abs(pixel_delta.x), abs(pixel_delta.y));

  vec2 pixel_step = pixel_delta/steps;

  if (steps < 1.f) return RAY_MISS;

  vec2 uv_step = uv_delta/steps;

  float prev_ray_dist = ray_vec_intersection(origin, dir, normalize(oct_to_sphere(uv_start)));
  float start_geom_dist = texelFetch(probe_dist, ivec3(floor(p_start), pid), 0).r;


  for (float s = 0; s < steps; s += 1.f) {

    vec2 uv = uv_start + min(s + 0.5, steps) * uv_step;
    vec2 pixel_pos = p_start + s * pixel_step;

    float geom_dist = texelFetch(probe_dist, ivec3(floor(pixel_pos), pid), 0).r;

    vec3 next_dir = normalize(oct_to_sphere(uv_start + min(s + 1, steps) * uv_step));
    float next_ray_dist = ray_vec_intersection(origin, dir, next_dir);
    
    float max_ray_dist = max(prev_ray_dist, next_ray_dist);
    float min_ray_dist = min(prev_ray_dist, next_ray_dist);

    const float THIKNESS = 0.02;
    const float BIAS = 0.0;

    if (max_ray_dist > geom_dist) {
      
      vec3 norm = texelFetch(probe_norm, ivec3(uv * TEX_SIZE, pid), 0).xyz;

      if (min_ray_dist <= geom_dist + THIKNESS && dot(norm, dir) < 0) {
        vec3 probe_to_point = normalize(oct_to_sphere(uv));
        vec3 hit_r = geom_dist * probe_to_point;
        out_t = dot(hit_r - origin, dir)/dot(dir, dir);
        return RAY_HIT;
      } else {
        vec2 prev_uv = uv_start + s * uv_step;
        vec3 prev_dir = normalize(oct_to_sphere(prev_uv));
        float prev_ray_dist = ray_vec_intersection(origin, dir, prev_dir);
        vec3 prev_r = prev_ray_dist * prev_dir;
        float t = dot(prev_r - origin, dir)/dot(dir, dir);
        out_t = t;
        return RAY_UNKNOWN;
      }      
    }
    
    
    prev_ray_dist = next_ray_dist;
  }

  return RAY_MISS;
}


bool is_degenerate_segment(float a, float b) {
  return isnan(a) || isnan(b) || isinf(a) || isinf(b) || abs(a - b) <= 0.0001;
}

uint trace_probe(uint probe_id, vec3 origin, vec3 dir, float min_t, float max_t, out float out_t) {

  vec3 probe_pos = lightfield.positions[probe_id].xyz;

  vec3 origin_probe = origin - probe_pos;
  vec3 a = abs(origin_probe);

  if (dot(origin_probe, origin_probe) < 0.001) {
    return RAY_UNKNOWN;
  }

  vec3 borders = -(origin_probe + min_t * dir)/dir;

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
  vec3 start = origin_probe;
  for (uint i = 1; i < 5; i++) {

    if (is_degenerate_segment(segments[i-1], segments[i])) {
      continue;
    }

    const float RAY_OFFS = 0;//0.00005;

    vec3 start = origin_probe + (segments[i - 1] + RAY_OFFS) * dir;
    vec3 end = origin_probe + (segments[i] - RAY_OFFS) * dir;
    
    vec2 start_oct = sphere_to_oct(start);
    vec2 end_oct = sphere_to_oct(end);
    
    float t;
    uint res = trace_segment(probe_id, origin_probe, normalize(dir), start_oct, end_oct, t);    

    if (res == RAY_HIT || res == RAY_UNKNOWN) {
      out_t = t;
      return res;
    }
  }

  return RAY_MISS;
}


uint trace_ray(vec3 origin, vec3 dir, float min_t, float max_t, out vec3 out_ray, out uint hit_probe) {
  float dir_len = length(dir);
  dir /= dir_len; //normalize
  min_t = 0.f;
  max_t *= dir_len;

  uint res = RAY_UNKNOWN;
  vec3 ray;
  uvec3 start_probe = closest_probe(origin);
  
  uint offs = 0;
  for (uint i = 0; i < 8; i++) {
    uint id = next_probe(start_probe, offs);
    float t;
    res = trace_probe(id, origin, dir, min_t, max_t, t);
    
    if (res == RAY_HIT || res == RAY_MISS) {
      out_ray = ray;
      hit_probe = id;
      return res;
    }

    offs = (offs + 3) & 7; 
    //min_t = max(t, min_t);
  }


  return RAY_UNKNOWN;
}

bool draw_probes(vec3 camera, vec3 world_pos) {
  
  float pixel_dist = length(world_pos - camera);
  vec3 dir = (world_pos - camera)/pixel_dist;
  uint probes_count = min(MAX_PROBES, uint(lightfield.dim.w));
  const float R = 0.05f;
  for (uint i = 0; i < probes_count; i++) {
    float t;
    if (ray_sphere_intersection(camera, dir, lightfield.positions[i].xyz, R, t)) {
      if (t < pixel_dist) {
        return true;
      }
    }
  }
  return false;
}*/