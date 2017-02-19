<%import pre.fsh/>
<%decl_fragcolor/>

precision highp float;

void main(void)
{
  discard;
  vec2 p = gl_FragCoord.xy / 256.0;
  if (p.x >= 1.0 || p.y >= 1.0) {
    discard;
    // <%fragcolor/> = vec4(1.0);
    // return;
  }
  float i = 1;
  float v = fract(i / 256.0);
  <%fragcolor/> = vec4(v, v, v, 0.5);
}

