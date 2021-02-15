#ifndef RAYTRACING_GLSL_INCLUDED
#define RAYTRACING_GLSL_INCLUDED

//ray-tracing.ru
bool ray_sphere_intersection(vec3 origin, vec3 dir, vec3 s_center, float r, out float out_t) {
  vec3 k = origin - s_center;
  float b = dot(k, dir);
  float c = dot(k, k) - r*r;
  float d = b*b - c;

  if (d >= 0) {
    float sqrtfd = sqrt(d);
    // t, a == 1
    float t1 = -b + sqrtfd;
    float t2 = -b - sqrtfd;
    float min_t = min(t1,t2);
    float max_t = max(t1,t2);
    float t = (min_t >= 0) ? min_t : max_t;
    out_t = t;
    return (t > 0);
  }
  return false;
}

#endif