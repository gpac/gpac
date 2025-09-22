// Author: haiyoucuv
// License: MIT

#define PI 3.1415926

vec2 rotate2D(in vec2 uv, in float angle){
  
  return uv * mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
}
vec4 transition (vec2 uv) {
  
  vec2 p = fract(rotate2D(uv - 0.5, progress * PI * 2.0) + 0.5);

  return mix(
    getFromColor(p),
    getToColor(p),
    progress
  );
}
