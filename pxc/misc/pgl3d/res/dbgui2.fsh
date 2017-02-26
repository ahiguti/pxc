<%import pre.fsh/>
<%decl_fragcolor/>

precision highp float;

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
  vec2 p = (gl_FragCoord.xy + 0.5) / 256.0;
  p.y -= 0.25;
  if (p.x >= 1.0 || p.y >= 1.0 || p.y < 0.0) {
    discard;
  }
  <%fragcolor/> = test_precision(p);
}

