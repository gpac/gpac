// author: Brandon Anzaldi
// license: MIT
uniform float offset; // = 0.05

// Pseudo-random noise function
// http://byteblacksmith.com/improvements-to-the-canonical-one-liner-glsl-rand-for-opengl-es-2-0/
highp float noise(vec2 co)
{
    highp float a = 12.9898;
    highp float b = 78.233;
    highp float c = 43758.5453;
    highp float dt= dot(co.xy * progress, vec2(a, b));
    highp float sn= mod(dt,3.14);
    return fract(sin(sn) * c);
}

vec4 transition(vec2 p) {
  if (progress < offset) {
    return getFromColor(p);
  } else if (progress > (1.0 - offset)) {
    return getToColor(p);
  } else {
    return vec4(vec3(noise(p)), 1.0);
  }
}
