#ifndef TRACE_PROBE_GLSL
#define TRACE_PROBE_GLSL

#include "oct_coord.glsl"

#define MAX_PROBES 256

#define LIGHTFIELD_SET 1
#define LIGHTFIELD_DATA_BIND 0
#define LIGHTFIELD_PROBE_DIST_BIND 1
#define LIGHTFIELD_PROBE_NORM_BIND 2
#define LIGHTFIELD_LOW_RES_BIND 3

layout(set = LIGHTFIELD_SET, binding = LIGHTFIELD_DATA_BIND)
uniform LightFieldData {
  vec4 probe_count;
  vec4  probe_start;
  vec4  probe_step;
  vec4 positions[MAX_PROBES];
} lightfield;

layout(set = LIGHTFIELD_SET, binding = LIGHTFIELD_PROBE_DIST_BIND)
uniform sampler2DArray probe_dist;

layout(set = LIGHTFIELD_SET, binding = LIGHTFIELD_PROBE_NORM_BIND)
uniform sampler2DArray probe_norm;

layout(set = LIGHTFIELD_SET, binding = LIGHTFIELD_LOW_RES_BIND)
uniform sampler2DArray probe_low_res;

layout(set = LIGHTFIELD_SET, binding = 4)
uniform sampler2DArray probe_radiance;

layout(set = LIGHTFIELD_SET, binding = 5)
uniform sampler2DArray probe_irradiance;

#define TRACE_RESULT_MISS    0
#define TRACE_RESULT_HIT     1
#define TRACE_RESULT_UNKNOWN 2

const vec2 TEX_SIZE       = vec2(1024.0);
const vec2 TEX_SIZE_SMALL = vec2(64.0);

const vec2 INV_TEX_SIZE       = vec2(1.0) / TEX_SIZE;
const vec2 INV_TEX_SIZE_SMALL = vec2(1.0) / TEX_SIZE_SMALL;

const float MIN_THICKNESS = 0.03; // meters
const float MAX_THICKNESS = 0.50; // meters

float dist_squared(vec2 v0, vec2 v1) {
  vec2 t = v0 - v1;
  return dot(t, t);
}

float dist_squared(vec3 v0, vec3 v1) {
  vec3 t = v0 - v1;
  return dot(t, t);
}

float norminf(in vec2 v) {
  vec2 a = abs(v);
  return max(a.x, a.y);
}

int grid_to_index(in vec3 p) {
  return int(p.z + p.y * lightfield.probe_count.z + 
    p.x * lightfield.probe_count.z * lightfield.probe_count.y);
}

ivec3 base_grid_coord(vec3 p) {
  return clamp(ivec3(p - lightfield.probe_start.xyz), 
    ivec3(0, 0, 0), ivec3(lightfield.probe_count.xyz) - ivec3(1, 1, 1));
}

vec3 nearest_probe(vec3 p) {
  vec3 max_coord = vec3(lightfield.probe_count.xyz) - vec3(1, 1, 1);
  vec3 float_coord = (p - lightfield.probe_start.xyz)/lightfield.probe_step.xyz;
  vec3 base_coord = clamp(floor(float_coord), vec3(0, 0, 0), max_coord);

  return base_coord;
}

int next_index(vec3 p, int i) {
  vec3 max_coord = vec3(lightfield.probe_count.xyz) - vec3(1, 1, 1);
  return grid_to_index(clamp(p + vec3(i & 1, (i >> 1) & 1, (i >> 2) & 1), vec3(0), max_coord));
}

vec3 calc_probe_pos(vec3 probe) {
  return lightfield.probe_start.xyz + probe * lightfield.probe_step.xyz;
}

vec3 get_probe_position(uint index) {
  return lightfield.positions[index].xyz;
}

void swap_min(inout float a, inout float b) {
  float temp = min(a, b);
  b = max(a, b);
  a = temp;
}

void compute_trace_segments(in vec3 origin, in vec3 dir_frac, in float tmin, in float tmax, out float bounds[5]) {
  bounds[0] = tmin;

  vec3 t = origin * (-dir_frac);
  
  swap_min(t.x, t.y);
  swap_min(t.y, t.z);
  swap_min(t.x, t.y);

  bounds[1] = clamp(t.x, tmin, tmax);
  bounds[2] = clamp(t.y, tmin, tmax);
  bounds[3] = clamp(t.z, tmin, tmax);

  bounds[4] = tmax;
}

float dist_to_intersection(in vec3 o, in vec3 d, in vec3 v) {
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
/*
float dist_to_intersection(in vec3 o, in vec3 d, in vec3 v) {
  float numer;
  float denom = v.y * d.z - v.z * d.y;

  if (abs(denom) > 0.1) {
    numer = o.y * d.z - o.z * d.y;
  } else {
        // We're in the yz plane; use another one
    numer = o.x * d.y - o.y * d.x;
    denom = v.x * d.y - v.y * d.x;
  }

  return numer / denom;
}*/

bool trace_hi_lowres(
  in vec3 ray_origin,
  in vec3 ray_dir,
  in uint probe_id, 
  inout vec2 texc,
  in vec2 segment_end_texc,
  inout vec2 end_high_res_texc,
  inout float current_lod);

int trace_high_res(
  in vec3 ray_origin,
  in vec3 ray_dir,
  in vec2 start_texc,
  in vec2 end_texc,
  in uint probe_id,
  inout float tmin,
  inout float tmax, 
  inout vec2 hit_texc) {
  
  vec2 texc_delta = end_texc - start_texc;
  float texc_distance = length(texc_delta);

  vec2 texc_dir = texc_delta * (1.0/texc_distance);
  float texc_step = INV_TEX_SIZE.x * (texc_distance/norminf(texc_delta));
  
  vec3 dir_from_probe_before = oct_decode(start_texc);
  float distance_from_pbrobe_to_ray_before = max(0, dist_to_intersection(ray_origin, ray_dir, dir_from_probe_before));

  for (float d = 0.0f; d <= texc_distance; d += texc_step) {
    vec2 texc = texc_dir * min(d + 0.5 * texc_step, texc_distance) + start_texc;
    float dist_from_probe_to_surf = texelFetch(probe_dist, ivec3(TEX_SIZE * texc, probe_id), 0).r;
    vec3 dir_from_probe = oct_decode(texc);

    vec2 texc_after = texc_dir * min(d + texc_step, texc_distance) + start_texc;
    vec3 dir_from_probe_after = oct_decode(texc_after);
    float dist_from_probe_to_ray_after = dist_to_intersection(ray_origin, ray_dir, dir_from_probe_after);

    float max_dist_from_probe_to_ray = max(dist_from_probe_to_ray_after, distance_from_pbrobe_to_ray_before);

    if (max_dist_from_probe_to_ray >= dist_from_probe_to_surf) {
      float min_dist_from_probe_to_ray = min(dist_from_probe_to_ray_after, distance_from_pbrobe_to_ray_before);
      float dist_from_probe_to_ray = 0.5 * (min_dist_from_probe_to_ray + max_dist_from_probe_to_ray);

      vec3 probe_hit_point = dist_from_probe_to_surf * dir_from_probe;
      float dist_along_ray = dot(probe_hit_point - ray_origin, ray_dir);
      vec3 normal = texelFetch(probe_norm, ivec3(TEX_SIZE * texc, probe_id), 0).rgb;

      float surface_thickness = MIN_THICKNESS + (MAX_THICKNESS - MIN_THICKNESS) *
        max(dot(ray_dir, dir_from_probe), 0) * (2 - abs(dot(ray_dir, normal))) * clamp(dist_along_ray * 0.1, 0.05, 1.0);
      
      if (min_dist_from_probe_to_ray < dist_from_probe_to_surf + surface_thickness && dot(normal, ray_dir) < 0) {
        tmax = dist_along_ray;
        hit_texc = texc;
        return TRACE_RESULT_HIT;
      } else {
        vec3 point_before = dir_from_probe_before * distance_from_pbrobe_to_ray_before;
        float dist_along_ray_before = dot(point_before - ray_origin, ray_dir);
        tmin = max(tmin, min(dist_along_ray, dist_along_ray_before));
        return TRACE_RESULT_UNKNOWN;
      }
    }
    distance_from_pbrobe_to_ray_before = dist_from_probe_to_ray_after;
  }
  return TRACE_RESULT_MISS;
}

bool trace_low_res(
  in vec3 ray_origin,
  in vec3 ray_dir,
  in uint probe_id, 
  inout vec2 texc,
  in vec2 segment_end_texc,
  inout vec2 end_high_res_texc) {

  // Convert the texels to pixel coordinates:
  vec2 P0 = texc * TEX_SIZE_SMALL;
  vec2 P1 = segment_end_texc * TEX_SIZE_SMALL;

  // If the line is degenerate, make it cover at least one pixel
  // to avoid handling zero-pixel extent as a special case later
  P1 += vec2((dist_squared(P0, P1) < 0.0001) ? 0.01 : 0.0);
    // In pixel coordinates
  vec2 delta = P1 - P0;

    // Permute so that the primary iteration is in x to reduce
    // large branches later
  bool permute = false;
  if (abs(delta.x) < abs(delta.y)) {
    // This is a more-vertical line
    permute = true;
    delta = delta.yx; P0 = P0.yx; P1 = P1.yx;
  }

  float   stepDir = sign(delta.x);
  float   invdx = stepDir / delta.x;
  vec2 dP = vec2(stepDir, delta.y * invdx);

  vec3 initialDirectionFromProbe = oct_decode(texc);
  float prevRadialDistMaxEstimate = max(0.0, dist_to_intersection(ray_origin, ray_dir, initialDirectionFromProbe));
    // Slide P from P0 to P1
  float  end = P1.x * stepDir;

  float absInvdPY = 1.0 / abs(dP.y);

    // Don't ever move farther from texCoord than this distance, in texture space,
    // because you'll move past the end of the segment and into a different projection
  float maxTexCoordDistance = dot(segment_end_texc - texc, segment_end_texc - texc);

  for (vec2 P = P0; ((P.x * sign(delta.x)) <= end); ) {

    vec2 hitPixel = permute ? P.yx : P;

    float sceneRadialDistMin = texelFetch(probe_low_res, ivec3(hitPixel, probe_id), 0).r;

    // Distance along each axis to the edge of the low-res texel
    vec2 intersectionPixelDistance = (sign(delta) * 0.5 + 0.5) - sign(delta) * fract(P);

        // abs(dP.x) is 1.0, so we skip that division
        // If we are parallel to the minor axis, the second parameter will be inf, which is fine
    float rayDistanceToNextPixelEdge = min(intersectionPixelDistance.x, intersectionPixelDistance.y * absInvdPY);

        // The exit coordinate for the ray (this may be *past* the end of the segment, but the
        // callr will handle that)
    end_high_res_texc = (P + dP * rayDistanceToNextPixelEdge) * INV_TEX_SIZE_SMALL;
    end_high_res_texc = permute ? end_high_res_texc.yx : end_high_res_texc;

    if (dot(end_high_res_texc - texc, end_high_res_texc - texc) > maxTexCoordDistance) {
      // Clamp the ray to the segment, because if we cross a segment boundary in oct space
      // then we bend the ray in probe and world space.
      end_high_res_texc = segment_end_texc;
    }

        // Find the 3D point *on the trace ray* that corresponds to the tex coord.
        // This is the intersection of the ray out of the probe origin with the trace ray.
    vec3 directionFromProbe = oct_decode(end_high_res_texc);
    float distanceFromProbeToRay = max(0.0, dist_to_intersection(ray_origin, ray_dir, directionFromProbe));

    float maxRadialRayDistance = max(distanceFromProbeToRay, prevRadialDistMaxEstimate);
    prevRadialDistMaxEstimate = distanceFromProbeToRay;

    if (sceneRadialDistMin < maxRadialRayDistance) {
      // A conservative hit.
      //
      //  -  endHighResTexCoord is already where the ray would have LEFT the texel
      //     that created the hit.
      //
      //  -  texCoord should be where the ray entered the texel
      texc = (permute ? P.yx : P) * INV_TEX_SIZE_SMALL;
      return true;
    }

    // Ensure that we step just past the boundary, so that we're slightly inside the next
    // texel, rather than at the boundary and randomly rounding one way or the other.
    const float epsilon = 0.001; // pixels
    P += dP * (rayDistanceToNextPixelEdge + epsilon);
  } // for each pixel on ray

  // If exited the loop, then we went *past* the end of the segment, so back up to it (in practice, this is ignored
  // by the caller because it indicates a miss for the whole segment)
  texc = segment_end_texc;
  return false;
}

int trace_segment(
  in vec3 ray_origin,
  in vec3 ray_dir,
  in float t0,
  in float t1,
  in int probe_id,
  inout float tmin,
  inout float tmax,
  inout vec2 hit_texc) {
  
  const float RAY_EPS = 0.001;
  vec3 probe_start = ray_origin + ray_dir * (t0 + RAY_EPS);
  vec3 probe_end = ray_origin + ray_dir * (t1 - RAY_EPS);

  if (dist_squared(probe_start, probe_end) < 0.001) {
    probe_start = ray_dir;
  }

  vec2 start_oct = oct_encode(normalize(probe_start));
  vec2 end_oct = oct_encode(normalize(probe_end));
  vec2 texc = start_oct;
  vec2 segment_end = end_oct;
  float lod = 5.f;
  for (int i = 0; i < 32; i++) {
    vec2 end_texc = segment_end;
    if (!trace_hi_lowres(ray_origin, ray_dir, probe_id, texc, segment_end, end_texc, lod)) {
      return TRACE_RESULT_MISS;
    } else {
      int result = trace_high_res(ray_origin, ray_dir, texc, end_texc, probe_id, tmin, tmax, hit_texc);
      if (result != TRACE_RESULT_MISS) {
        return result;
      }
    }

    lod = 5.f;
    vec2 texc_dir = normalize(segment_end - texc);
    if (dot(texc_dir, segment_end - texc) <= INV_TEX_SIZE.x) {
      return TRACE_RESULT_MISS;
    } else {
      texc = end_texc + texc_dir * TEX_SIZE.x * 0.1;
    }
  }

  return TRACE_RESULT_MISS;
}

int trace_probe(
  in vec3 ray_origin,
  in vec3 ray_dir, 
  in int probe_id,
  inout float tmin,
  inout float tmax,
  inout vec2 hitexc) {
  
  const float degenerateEpsilon = 0.001;
  vec3 probeOrigin = get_probe_position(probe_id);

  vec3 probeRayOrigin = ray_origin - probeOrigin;
  vec3 probeRayDir = normalize(ray_dir);

  float segments[5];
  compute_trace_segments(probeRayOrigin, 1.0/probeRayDir, tmin, tmax, segments);

  for (int i = 0; i < 4; i++) {
    if (abs(segments[i+1] - segments[i]) >= degenerateEpsilon) {
      int result = trace_segment(probeRayOrigin, probeRayDir, segments[i], segments[i+1], probe_id, tmin, tmax, hitexc);
      if (result != TRACE_RESULT_MISS) {
        return result;
      }
    }
  }
  return TRACE_RESULT_MISS;
}

int trace(
  in vec3 ray_origin,
  in vec3 ray_dir,
  inout float tmax,
  out vec2 hittexc,
  out int hit_probe) {
  
  hit_probe = -1;

  vec3 base_probe = nearest_probe(ray_origin);
  int i = 0;

  int probes_left = 8;
  float tmin = 0.f;
  
  int result = TRACE_RESULT_UNKNOWN;

  while (probes_left > 0) {
    int probe_id = next_index(base_probe, i);
    result = trace_probe(ray_origin, ray_dir, probe_id, tmin, tmax, hittexc);
    if (result == TRACE_RESULT_UNKNOWN) {
      i = (i + 3) & 7;
      probes_left--;
    } else {
      if (result == TRACE_RESULT_HIT) {
        hit_probe = probe_id;
      }
      break;
    }
  }

  return result;
}


vec3 computePrefilteredIrradiance(vec3 wsPosition, vec3 wsN) {
	vec3 baseProbe = nearest_probe(wsPosition);
	vec3 baseProbePos = calc_probe_pos(baseProbe);
	
  vec3 sumIrradiance = vec3(0.0);
	float sumWeight = 0.0;
	// Trilinear interpolation values along axes
	vec3 alpha = clamp((wsPosition - baseProbePos) / lightfield.probe_step.xyz, vec3(0), vec3(1));

	// Iterate over the adjacent probes defining the surrounding vertex "cage"
	for (int i = 0; i < 8; ++i) {
		// Compute the offset grid coord and clamp to the probe grid boundary
		vec3 offset = vec3(ivec3(i, i >> 1, i >> 2) & ivec3(1));
		vec3 probeGridCoord = clamp(baseProbe + offset, vec3(0), lightfield.probe_count.xyz - vec3(1));
		int p = next_index(baseProbe, i);

		// Compute the trilinear weights based on the grid cell vertex to smoothly
		// transition between probes. Avoid ever going entirely to zero because that
		// will cause problems at the border probes.
		vec3 trilinear = mix(1.0 - alpha, alpha, offset);
		float weight = trilinear.x * trilinear.y * trilinear.z;

		// Make cosine falloff in tangent plane with respect to the angle from the surface to the probe so that we never
		// test a probe that is *behind* the surface.
		// It doesn't have to be cosine, but that is efficient to compute and we must clip to the tangent plane.
		vec3 probePos = calc_probe_pos(probeGridCoord);
		vec3 probeToPoint = wsPosition - probePos;
		vec3 dir = normalize(-probeToPoint);

		// Smooth back-face test
		weight *= max(0.05, dot(dir, wsN));
		// Avoid zero weight
		weight = max(0.0002, weight);

		sumWeight += weight;

		vec3 irradianceDir = normalize(wsN);
		vec2 octUV = oct_encode(irradianceDir);

		vec3 probeIrradiance = texture(probe_irradiance, vec3(octUV, p)).rgb;

		// Debug probe contribution by visualizing as colors
		// probeIrradiance = 0.5 * probeIndexToColor(lightFieldSurface, p);

		sumIrradiance += weight * probeIrradiance;
	}

	return 2.0 * PI * sumIrradiance / sumWeight;
}

bool trace_hi_lowres(
  in vec3 ray_origin,
  in vec3 ray_dir,
  in uint probe_id, 
  inout vec2 texc,
  in vec2 segment_end_texc,
  inout vec2 end_high_res_texc,
  inout float current_lod)
{
  vec2 P0 = texc * TEX_SIZE;
  vec2 P1 = segment_end_texc * TEX_SIZE;

  P1 += vec2((dist_squared(P0, P1) < 0.0001) ? 0.01 : 0.0);

  vec2 delta = P1 - P0;

  bool permute = false;
  if (abs(delta.x) < abs(delta.y)) {
    permute = true;
    delta = delta.yx; P0 = P0.yx; P1 = P1.yx;
  }

  float   step_dir = sign(delta.x);
  float   invdx = step_dir / delta.x;
  vec2 dP = vec2(step_dir, delta.y * invdx);

  vec3 initial_direction = oct_decode(texc);
  float prev_ray_distance = max(0.0, dist_to_intersection(ray_origin, ray_dir, initial_direction));

  float  end = P1.x * step_dir;

  float abs_inv_dPy = 1.0 / abs(dP.y);

  float max_texcoord_distance = dot(segment_end_texc - texc, segment_end_texc - texc);

  int iterations = 128;
  for (vec2 P = P0; ((P.x * sign(delta.x)) <= end) && iterations > 0; ) {
    
    float inv_lod_scale = exp2(-current_lod);
    float lod_scale = exp2(current_lod);

    vec2 hit_pixel = inv_lod_scale * (permute ? P.yx : P);
    float scene_dist = texelFetch(probe_dist, ivec3(hit_pixel, probe_id), int(current_lod)).r;

    vec2 pixel_edge_dist = (sign(delta) * 0.5 + 0.5) - sign(delta) * fract(hit_pixel);
    float dist_to_edge = min(pixel_edge_dist.x, pixel_edge_dist.y * abs_inv_dPy);
    vec2 hit_edge = hit_pixel + dP * dist_to_edge;

    end_high_res_texc = hit_edge * lod_scale * INV_TEX_SIZE;
    end_high_res_texc = permute ? end_high_res_texc.yx : end_high_res_texc;

    if (dot(end_high_res_texc - texc, end_high_res_texc - texc) > max_texcoord_distance) {
      end_high_res_texc = segment_end_texc;
    }

    vec3 probe_dir = oct_decode(end_high_res_texc);
    float ray_dist = max(0.0, dist_to_intersection(ray_origin, ray_dir, probe_dir));

    float max_dist = max(ray_dist, prev_ray_distance);
    float min_dist = min(ray_dist, prev_ray_distance);
    

    if (scene_dist < max_dist) {
      current_lod = (min_dist > scene_dist)? 0.f : (current_lod - 1.f);
      texc = (permute ? P.yx : P) * INV_TEX_SIZE;
      if (current_lod <= 0.f) {
        return true;
      }
    } else {
      prev_ray_distance = ray_dist;
      const float epsilon = 0.001; // pixels
      P += dP * (dist_to_edge * lod_scale + epsilon);
    }

    iterations--;
  }

  texc = segment_end_texc;
  return false;
}

int trace_hi_segment(
  in vec3 ray_origin,
  in vec3 ray_dir,
  in float t0,
  in float t1,
  in int probe_id,
  inout float tmin,
  inout float tmax,
  inout vec2 hit_texc)
{  
  const float RAY_EPS = 0.001;

  float t_start = t0 + RAY_EPS;
  float t_end = t1 - RAY_EPS;

  vec2 start_uv = oct_encode(normalize(ray_origin + ray_dir * t_start));
  vec2 end_uv = oct_encode(normalize(ray_origin + ray_dir * t_end));
  
  float current_lod = 2.f;

  vec2 P0 = start_uv * TEX_SIZE;
  vec2 P1 = end_uv * TEX_SIZE;

  vec2 delta = P1 - P0;

    // Permute so that the primary iteration is in x to reduce
    // large branches later
  bool permute = false;
  if (abs(delta.x) < abs(delta.y)) {
    permute = true;
    delta = delta.yx;
    P0 = P0.yx;
  }

  float   stepDir = sign(delta.x);
  float   invdx = stepDir / delta.x;
  vec2 dP = vec2(stepDir, delta.y * invdx);
  float absInvdPY = 1.0 / abs(dP.y);

  vec2 P = P0;

  int last_state = TRACE_RESULT_MISS;
  int iterations = 64;

  while (current_lod >= 0 && iterations > 0) {
    //1. Get P's cell -> reconstruct plane and get t_bound
    //2 Calculate t_instersection intersection beetween ray and plane
    //3. if plane not hit current_lod += 2, t = t_bound + eps, P = project(t)
    //4 else t = t_intersection, P = prohect (t)

    float scale_inv = exp2(-current_lod); 
    vec3 current_dir = oct_decode(P * INV_TEX_SIZE);
    float d1 = dist_to_intersection(ray_origin, ray_dir, current_dir);
    
    float t = dot(current_dir * d1 - ray_origin, ray_dir);
    vec2 cell = P * scale_inv;
    
    vec2 tex_distances = (sign(delta) * 0.5 + 0.5) - sign(delta) * fract(cell);
    vec2 cell_end = cell + dP * min(tex_distances.x, tex_distances.y * absInvdPY);
    vec2 P_bound = P + (dP * min(tex_distances.x, tex_distances.y * absInvdPY) + 0.001) * exp2(current_lod);

    vec2 end = (permute?  cell_end.yx : cell_end.xy) * exp2(current_lod) * INV_TEX_SIZE; //to [0; 1]

    vec3 end_dir = oct_decode(end);

    float end_distance = dist_to_intersection(ray_origin, ray_dir, end_dir);
    float t_bound = dot(end_dir * end_distance - ray_origin, ray_dir);

    t_bound = clamp(t_bound, t, t_end);

    vec2 cell_center = floor(permute? cell.yx : cell.xy) + vec2(0.5, 0.5);

    vec3 plane_direction = oct_decode(cell_center*exp2(current_lod)*INV_TEX_SIZE);
    float plane_distance = texelFetch(probe_dist, ivec3(cell_center, probe_id), int(current_lod)).r;
    plane_direction *= plane_distance;

    //plane intersection

    float D = dot(ray_origin, plane_direction);
    float Q = dot(ray_dir, plane_direction);

    if (abs(Q) < 1e-6) {
      return TRACE_RESULT_MISS;
    }

    float t_hit = (dot(plane_direction, plane_direction) - D)/Q;

    last_state = TRACE_RESULT_MISS;

    if (D < 1.f && t_hit >= t && t_hit < t_bound) {
      tmax = t_hit;
      P = oct_encode(ray_origin + t_hit * ray_dir) * TEX_SIZE;
      current_lod = min(current_lod + 2.f, 5.f);
      last_state = TRACE_RESULT_HIT;
    } else if (D < 1.f && t_hit < t) {
      tmin = t;
      current_lod = min(current_lod + 2.f, 5.f);
      last_state = TRACE_RESULT_UNKNOWN;
    } else if (D < 1.f && t_hit > t_bound) {
      P = P_bound;
      last_state = TRACE_RESULT_MISS;
    } else if (D > 1.f && t_hit < t) {
      P = P_bound;
      last_state = TRACE_RESULT_MISS;
    } else if (D > 1.f && t_hit > t) {
      tmin = t;
      current_lod = min(current_lod + 2.f, 5.f);
      last_state = TRACE_RESULT_UNKNOWN;
    }
  
    current_lod = min(current_lod - 1.f, 5.f);
    iterations--;
  }

  if (last_state == TRACE_RESULT_HIT) {
    hit_texc =  oct_encode(ray_origin + tmax * ray_dir);
  }

  return last_state;
}

int trace_hi_probe(
  in vec3 ray_origin,
  in vec3 ray_dir, 
  in int probe_id,
  inout float tmin,
  inout float tmax,
  inout vec2 hitexc) {
  
  const float degenerateEpsilon = 0.001;
  vec3 probeOrigin = get_probe_position(probe_id);

  vec3 probeRayOrigin = ray_origin - probeOrigin;
  vec3 probeRayDir = normalize(ray_dir);

  float segments[5];
  compute_trace_segments(probeRayOrigin, 1.0/probeRayDir, tmin, tmax, segments);

  for (int i = 0; i < 4; i++) {
    if (abs(segments[i+1] - segments[i]) >= degenerateEpsilon) {
      int result = trace_hi_segment(probeRayOrigin, probeRayDir, segments[i], segments[i+1], probe_id, tmin, tmax, hitexc);
      if (result != TRACE_RESULT_MISS) {
        return result;
      }
    }
  }
  return TRACE_RESULT_MISS;
}

int trace_hi(
  in vec3 ray_origin,
  in vec3 ray_dir,
  inout float tmax,
  out vec2 hittexc,
  out int hit_probe) {
  
  hit_probe = -1;

  vec3 base_probe = nearest_probe(ray_origin);
  int i = 0;

  int probes_left = 8;
  float tmin = 0.f;
  
  int result = TRACE_RESULT_UNKNOWN;

  while (probes_left > 0) {
    int probe_id = next_index(base_probe, i);
    result = trace_hi_probe(ray_origin, ray_dir, probe_id, tmin, tmax, hittexc);
    if (result == TRACE_RESULT_UNKNOWN) {
      i = (i + 3) & 7;
      probes_left--;
    } else {
      if (result == TRACE_RESULT_HIT) {
        hit_probe = probe_id;
      }
      break;
    }
  }

  return result;
}


bool draw_probes(vec3 camera, vec3 world_pos) {
  float pixel_dist = length(world_pos - camera);
  vec3 dir = (world_pos - camera)/pixel_dist;
  uint probes_count = min(MAX_PROBES, uint(lightfield.probe_count.x * lightfield.probe_count.y * lightfield.probe_count.z));
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
}

#endif