#ifndef REAL_SH_INCLUDED
#define REAL_SH_INCLUDED

#define PI 3.14159265359

#define NUM_COEFFS 36

struct Sample {
  float w[4];
  float sh[NUM_COEFFS];
};

struct ShProbe {
  float coeffs[NUM_COEFFS];
};

//https://github.com/google/spherical-harmonics/blob/master/sh/spherical_harmonics.cc
float y0(vec3 w) {
  // 0.5 * sqrt(1/pi)
  return 0.282095;
}

float y1n1(vec3 w) {
  // -sqrt(3/(4pi)) * y
  return -0.488603 * w.y;
}

float y10(vec3 w) {
  // sqrt(3/(4pi)) * z
  return 0.488603 * w.z;
}

float y11(vec3 w) {
  // -sqrt(3/(4pi)) * x
  return -0.488603 * w.x;
}

float y2n2(vec3 w) {
  // 0.5 * sqrt(15/pi) * x * y
  return 1.092548 * w.x * w.y;
}

float y2n1(vec3 w) {
   // -0.5 * sqrt(15/pi) * y * z
  return -1.092548 * w.y * w.z;
}

float y20(vec3 w) {
  // 0.25 * sqrt(5/pi) * (-x^2-y^2+2z^2)
  return 0.315392 * (-w.x * w.x - w.y * w.y + 2.0 * w.z * w.z); 
}

float y21(vec3 w) {
   // -0.5 * sqrt(15/pi) * x * z
  return -1.092548 * w.x * w.z;
}

float y22(vec3 w) {
  // 0.25 * sqrt(15/pi) * (x^2 - y^2)
  return 0.546274 * (w.x * w.x - w.y * w.y);
}

float y3n3(vec3 w) {
  // -0.25 * sqrt(35/(2pi)) * y * (3x^2 - y^2)
  return -0.590044 * w.y * (3.0 * w.x * w.x - w.y * w.y);
}

float y3n2(vec3 w) {
  // 0.5 * sqrt(105/pi) * x * y * z
  return 2.890611 * w.x * w.y * w.z;
}

float y3n1(vec3 w) {
  // -0.25 * sqrt(21/(2pi)) * y * (4z^2-x^2-y^2)
  return -0.457046 * w.y * (4.0 * w.z * w.z - w.x * w.x
                             - w.y * w.y);
}

float y30(vec3 w) {
  // 0.25 * sqrt(7/pi) * z * (2z^2 - 3x^2 - 3y^2)
  return 0.373176 * w.z * (2.0 * w.z * w.z - 3.0 * w.x * w.x
                             - 3.0 * w.y * w.y);
}

float y31(vec3 w) {
  // -0.25 * sqrt(21/(2pi)) * x * (4z^2-x^2-y^2)
  return -0.457046 * w.x * (4.0 * w.z * w.z - w.x * w.x
                             - w.y * w.y);
}

float y32(vec3 w) {
  // 0.25 * sqrt(105/pi) * z * (x^2 - y^2)
  return 1.445306 * w.z * (w.x * w.x - w.y * w.y);
}

float y33(vec3 w) {
  // -0.25 * sqrt(35/(2pi)) * x * (x^2-3y^2)
  return -0.590044 * w.x * (w.x * w.x - 3.0 * w.y * w.y);
}


float y4n4(vec3 w) {
  // 0.75 * sqrt(35/pi) * x * y * (x^2-y^2)
  return 2.503343 * w.x * w.y * (w.x * w.x - w.y * w.y);
}

float y4n3(vec3 w) {
  // -0.75 * sqrt(35/(2pi)) * y * z * (3x^2-y^2)
  return -1.770131 * w.y * w.z * (3.0 * w.x * w.x - w.y * w.y);
}

float y4n2(vec3 w) {
  // 0.75 * sqrt(5/pi) * x * y * (7z^2-1)
  return 0.946175 * w.x * w.y * (7.0 * w.z * w.z - 1.0);
}

float y4n1(vec3 w) {
  // -0.75 * sqrt(5/(2pi)) * y * z * (7z^2-3)
  return -0.669047 * w.y * w.z * (7.0 * w.z * w.z - 3.0);
}

float y40(vec3 w) {
  // 3/16 * sqrt(1/pi) * (35z^4-30z^2+3)
  float z2 = w.z * w.z;
  return 0.105786 * (35.0 * z2 * z2 - 30.0 * z2 + 3.0);
}

float y41(vec3 w) {
  // -0.75 * sqrt(5/(2pi)) * x * z * (7z^2-3)
  return -0.669047 * w.x * w.z * (7.0 * w.z * w.z - 3.0);
}

float y42(vec3 w) {
  // 3/8 * sqrt(5/pi) * (x^2 - y^2) * (7z^2 - 1)
  return 0.473087 * (w.x * w.x - w.y * w.y)
      * (7.0 * w.z * w.z - 1.0);
}

float y43(vec3 w) {
  // -0.75 * sqrt(35/(2pi)) * x * z * (x^2 - 3y^2)
  return -1.770131 * w.x * w.z * (w.x * w.x - 3.0 * w.y * w.y);
}

float y44(vec3 w) {
  // 3/16*sqrt(35/pi) * (x^2 * (x^2 - 3y^2) - y^2 * (3x^2 - y^2))
  float x2 = w.x * w.x;
  float y2 = w.y * w.y;
  return 0.625836 * (x2 * (x2 - 3.0 * y2) - y2 * (3.0 * x2 - y2));
}

float pow4(float a) {
  a *= a;
  return a*a;
}

float pow2(float a) {
  return a*a;
}

float y5n5(vec3 w) {
  return -3/32*sqrt(2*77/PI)*w.y*(5*pow4(w.x) - 10*pow2(w.x*w.y) + pow4(w.y));
}

float y5n4(vec3 w) {
  return 3/4 * sqrt(385/PI)*w.x*w.y*w.z*(w.x*w.x - w.y*w.y);
}

float y5n3(vec3 w) {
  return -sqrt(2 * 385/PI)/32 * w.y * (3*w.x*w.x - w.y*w.y)*(-1 + 9*w.z*w.z);
}

float y5n2(vec3 w) {
  return sqrt(1155/PI)/4 * w.x*w.y*w.z*(-1 + 3*w.z*w.z); 
}

float y5n1(vec3 w) {
  return -1/16 * sqrt(165/PI)*w.y*(-14*w.z*w.z + 21 * pow4(w.z) + 1);
}

float y50(vec3 w) {
  return 1/16*sqrt(11/PI) * (63*pow4(w.z) - 70*w.z*w.z + 15)*w.z;
}

float y51(vec3 w) {
  float z2 = w.z * w.z;
  float z4 = z2 * z2;
  return -sqrt(165/PI)/16 * w.x * (-14*z2 + 21*z4 + 1);
}

float y52(vec3 w) {
  return sqrt(1155/PI)/8 * (w.x*w.x - w.y*w.y)*w.z*(-1 + 3*w.z*w.z);
}

float y53(vec3 w) {
  return -sqrt(2*385/PI)/32*w.x*(w.x*w.x - 3*w.y*w.y)*(-1 + 9*w.z*w.z);
}

float y54(vec3 w) {
  return 3/16*sqrt(385/PI)*w.z*(pow4(w.x) - 6*pow2(w.x*w.y) + pow4(w.y));
}

float y55(vec3 w) {
  return -3/32*sqrt(2*77/PI)*w.x*(pow4(w.x) - 10*pow2(w.x*w.y) + 5*pow4(w.y));
}

float sample_sh25(in ShProbe probe, vec3 w) {
  float sh_distance = 0.f;
  #define E(i) probe.coeffs[i]
  sh_distance += E(0) * y0(w);
  sh_distance += E(1) * y1n1(w) + E(2) * y10(w) + E(3) * y11(w);
  sh_distance += E(4) * y2n2(w) + E(5) * y2n1(w) + E(6) * y20(w) + E(7) * y21(w) + E(8) * y22(w);
  sh_distance += E(9) * y3n3(w) + E(10) * y3n2(w) + E(11) * y3n1(w) + E(12) * y30(w) + E(13) * y31(w) + E(14) * y32(w) + E(15) * y33(w);
  sh_distance += E(16)*y4n4(w) + E(17)*y4n3(w) + E(18)*y4n2(w) + E(19)*y4n1(w) + E(20)*y40(w) + E(21)*y41(w) + E(22)*y42(w);
  sh_distance += E(23)*y43(w) + E(24)*y44(w);
  #undef E
  return sh_distance;
}

#if NUM_COEFFS >= 36
float sample_sh36(in ShProbe probe, vec3 w) {
  float sh_distance = 0.f;
  #define E(i) probe.coeffs[i]
  sh_distance += E(0) * y0(w);
  sh_distance += E(1) * y1n1(w) + E(2) * y10(w) + E(3) * y11(w);
  sh_distance += E(4) * y2n2(w) + E(5) * y2n1(w) + E(6) * y20(w) + E(7) * y21(w) + E(8) * y22(w);
  sh_distance += E(9) * y3n3(w) + E(10) * y3n2(w) + E(11) * y3n1(w) + E(12) * y30(w) + E(13) * y31(w) + E(14) * y32(w) + E(15) * y33(w);
  sh_distance += E(16)*y4n4(w) + E(17)*y4n3(w) + E(18)*y4n2(w) + E(19)*y4n1(w) + E(20)*y40(w) + E(21)*y41(w) + E(22)*y42(w);
  sh_distance += E(23)*y43(w) + E(24)*y44(w);
  sh_distance += E(25) * y5n5(w) + E(26) * y5n4(w) + E(27) * y5n3(w) + E(28) * y5n2(w);
  sh_distance += E(29) * y5n1(w) + E(30) * y50(w) + E(31) * y51(w) + E(32) * y52(w) + E(33)*y53(w);
  sh_distance += E(34) * y54(w) + E(35) * y55(w); 
  #undef E
  return sh_distance;
}
#endif 

#undef PI

#endif