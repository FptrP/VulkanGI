#ifndef BRDF_GLSL_INCLUDED
#define BRDF_GLSL_INCLUDED

const float PI = 3.14159265359;

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / max(denom, 0.003);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / max(denom, 0.001);
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
} 

vec3 calcFresnel(in vec3 eye_pos, in vec3 hit_pos, in vec3 hit_normal, in vec3 light_pos, in vec3 albedo, in float metallness, in float roughness) {
  vec3 V = normalize(eye_pos - hit_pos);
  vec3 N = normalize(hit_normal);
  
  vec3 F0 = vec3(0.04);
  F0 = mix(F0, albedo, metallness);

  vec3 L = normalize(light_pos - hit_pos);
  vec3 H = normalize(V + L);

  return fresnelSchlick(max(dot(H, V), 0.0), F0);
}

vec3 calc_color(in vec3 eye_pos, in vec3 pos, in vec3 normal, in vec3 albedo, in vec3 light_pos, in vec3 light_color, float metallic, float roughness) {

  vec3 V = normalize(eye_pos - pos);
  vec3 N = normalize(normal);

  float l_distance = max(length(light_pos - pos), 0.005);

  vec3 L = (light_pos - pos)/l_distance;
  vec3 H = normalize(L + V);
  float attenuation = 1/(l_distance * l_distance);
  vec3 radiance = light_color * attenuation;

  vec3 F0 = vec3(0.04); 
  F0 = mix(F0, albedo, metallic);
  vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0); 

  float NDF = DistributionGGX(N, H, roughness);       
  float G   = GeometrySmith(N, V, L, roughness);        

  vec3 numerator    = NDF * G * F;
  float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
  vec3 specular     = numerator / denominator;  

  vec3 kS = F;
  vec3 kD = vec3(1.0) - kS;
  
  kD *= 1.0 - metallic;	 
  
  float NdotL = max(dot(N, L), 0.0);        
  
  return (kD * albedo / PI + specular) * radiance * NdotL;
}

vec3 calc_lambertian(in vec3 eye_pos, in vec3 pos, in vec3 normal, in vec3 albedo, in vec3 light_pos, in vec3 light_color, float metallic, float roughness) {
  vec3 V = normalize(eye_pos - pos);
  vec3 N = normalize(normal);

  float l_distance = max(length(light_pos - pos), 0.015);

  vec3 L = (light_pos - pos)/l_distance;
  vec3 H = normalize(L + V);
  float attenuation = 1.f/(l_distance * l_distance);
  vec3 radiance = light_color * attenuation;

  vec3 F0 = vec3(0.04); 
  F0 = mix(F0, albedo, metallic);
  vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);  

  vec3 kS = F;
  vec3 kD = vec3(1.0) - kS;
  
  kD *= 1.0 - metallic;	 
  
  float NdotL = max(dot(N, L), 0.0);        
  
  return (kD*albedo/PI) * radiance * NdotL;
}

#endif