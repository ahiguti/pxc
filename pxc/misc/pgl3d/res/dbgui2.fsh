<%import pre.fsh/>
<%import pnoise.fsh/>
<%decl_fragcolor/>
<%frag_in/> vec2 vary_vert;

precision highp float;

float
test_noise_one(vec2 p)
{
  float z = (p.x + p.y) * 1.1 + 0.7;
  // z = 40.0;
  return pnoise3(vec3(p, z));
}

vec4
test_noise(vec2 p)
{
  float v0 = test_noise_one(p * 235.5 - 114.0);
  float v1 = test_noise_one(p * 188.7 - 10.3);
  float v2 = test_noise_one(p * 77.7 - 27.9);
  float v3 = test_noise_one(p * 56.2 - 4.2);
  float v4 = test_noise_one(p * 24.2);
  // float v = (v0 + v1 + v2 + v3 + v4) / 2.0;
  float v = (v0 + v1) / 2.0;
  return vec4(v, v, v, 1.0);
}

vec4
test_precision(vec2 p)
{
  p = floor(p * 8.0);
  int n = int(p.y * 8.0 + p.x);
  float v = 1.0;
  for (int i = 0; i < n; ++i) {
    v = v * 2.0;
  }
  float w = v + 1.0;
  if (v != w) {
    return vec4(0.0, float(n) / 64.0, 0.0, 1.0);
  } else {
    return vec4(float(n) / 64.0, 0.0, 0.0, 1.0);
  }
}

void main(void)
{
  <%fragcolor/> = test_noise(vary_vert);
  return;
  /*
  vec2 p = (gl_FragCoord.xy + 0.5) / 256.0;
  p.y -= 0.25;
  if (p.x >= 1.0 || p.y >= 1.0 || p.y < 0.0) {
    discard;
  }
  // <%fragcolor/> = test_precision(p);
  */
}

