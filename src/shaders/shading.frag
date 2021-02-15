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

layout(set = 1, binding = 1) uniform sampler probe_sampler;
layout(set = 1, binding = 2) uniform texture2D probe_dist[MAX_PROBES];

layout(set = 1, binding = 0) uniform LightFieldData {
  vec4 dim;
  vec4 bmin;
  vec4 bmax;
  vec4 positions[MAX_PROBES];
} lightfield;

layout (push_constant) uniform PushData {
  vec3 camera_origin;
} pc;

float pcf_cubemap(in samplerCube shadow_tex, vec3 dir, float dist);
float pcf_octmap(in sampler2D shadow_tex, vec3 dir, float dist);

uint closest_probe(vec3 point);
uint closest_probe_slow(vec3 point);
uint trace_ray(vec3 origin, vec3 dir, float min_t, float max_t, out vec3 out_ray);
uint trace_ray_simple(uint probe, vec3 origin, vec3 dir, float min_t, float max_t, out vec3 out_ray);

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

uint closest_probe(vec3 point) {
  vec3 center = 0.5f * (lightfield.bmin.xyz + lightfield.bmax.xyz);
  vec3 field_size = (lightfield.bmax.xyz - lightfield.bmin.xyz);
  uvec3 cell = uvec3(floor(clamp(point - center, vec3(0, 0, 0), field_size)/lightfield.dim.xyz));
  uvec3 udim = uvec3(lightfield.dim.xyz);


  uint min_index = cell.x * udim.x + cell.y * udim.y + cell.z * udim.z;
  float min_dist = length(point - lightfield.positions[min_index].xyz);

  for (uint x = 0; x < 1; x++) {
    for (uint y = 0; y < 1; y++) {
      for (uint z = 0; z < 1; z++) {
        uvec3 p = cell + uvec3(x, y, z);
        uint index = p.x * udim.x + p.y * udim.y + p.z * udim.z;
        float dist = length(point - lightfield.positions[index].xyz);
        if (dist < min_dist) {
          min_dist = dist;
          min_index = index;
        }
      }
    }
  }

  return min_index;
}

void min_swap(inout float a, inout float b) {
  float temp = min(a, b);
  b = max(a, b);
  a = temp;
}

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

uint trace_ray(vec3 origin, vec3 dir, float min_t, float max_t, out vec3 out_ray) {
  vec3 start = origin + dir * min_t;
  uint probe_id = closest_probe_slow(origin);

  vec3 probe_pos = lightfield.positions[probe_id].xyz;
  start = start - probe_pos;

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
    vec3 end = origin + segments[i] * dir - probe_pos;
    
    vec2 start_oct = sphere_to_oct(start);
    vec2 end_oct = sphere_to_oct(end);
    
    vec2 pixel_dist = abs(end_oct - start_oct) * tex_size;
    float steps = 2 * max(pixel_dist.x, pixel_dist.y);
  
    vec2 duv = (end_oct - start_oct)/steps;
    vec3 dray = (end - start)/steps;

    for (float s = 0; s < steps; s++) {
      vec2 uv = start_oct + s * duv;
      vec3 r = start + s * dray; //Problem - uv != projected r. 
      float dist = texelFetch(sampler2D(probe_dist[probe_id], probe_sampler), ivec2(uv * tex_size), 0).r;
      vec3 oct_to_point = normalize(oct_to_sphere(uv));
      float ray_dist = ray_intersection_dist(start, end-start, oct_to_point);

      if (ray_dist > dist + 0.5) {
        out_ray  = oct_to_point * ray_dist + probe_pos;
        return RAY_UNKNOWN;
      }

      if (ray_dist >= dist - 0.001) {
        out_ray  = oct_to_point * ray_dist + probe_pos;
        return RAY_HIT;
      }
    }

    start = end;
  }

  return RAY_MISS;
}

uint trace_ray_simple(uint probe, vec3 origin, vec3 dir, float min_t, float max_t, out vec3 out_ray) {
  const float steps = 100000.f;
  float dt = (max_t - min_t)/steps;
  vec3 probe_pos = lightfield.positions[probe].xyz;

  for (float s = 0.f; s < steps; s += 1.f) {
    vec3 pos = origin + (min_t + s * dt) * dir - probe_pos;
    float probe_dist = texture(sampler2D(probe_dist[probe], probe_sampler), sphere_to_oct(pos)).r;
    float ray_dist = length(pos);

    if (ray_dist > probe_dist + 0.05) {
      out_ray = pos + probe_pos;
      return RAY_UNKNOWN;
    }

    if (ray_dist > probe_dist) {
      out_ray = pos + probe_pos;
      return RAY_HIT;
    }
  } 
  return RAY_MISS;
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