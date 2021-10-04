// Author:towrabbit
// License: MIT

float random (vec2 st) {
    return fract(sin(dot(st.xy,vec2(12.9898,78.233)))*43758.5453123);
}
vec4 transition (vec2 uv) {
  vec4 leftSide = getFromColor(uv);
  vec2 uv1 = uv;
  vec2 uv2 = uv;
  float uvz = floor(random(uv1)+progress);
  vec4 rightSide = getToColor(uv);
  float p = progress*2.0;
  return mix(leftSide,rightSide,uvz);
  return leftSide * ceil(uv.x*2.-p) + rightSide * ceil(-uv.x*2.+p);
}
