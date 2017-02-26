<%import>pre.fsh<%/>

<%decl_fragcolor/>
<%frag_in/> vec2 vary_vert;
uniform sampler2D sampler_sm[<%smsz/>];

int foo()
{
  for (int i = 0; i < 1; ++i) { }
  if (true) { return 1; }
  return 0;
}

void main(void)
{
discard;
  vec2 p = vary_vert.xy;
  vec4 v;
  v = <%texture2d/>(sampler_sm[0], (p - 0.5) / 1.0 + 0.5);
  float r = v.r;
  v = <%texture2d/>(sampler_sm[1], (p - 0.5) / 3.0 + 0.5);
  float g = v.r;
  v = <%texture2d/>(sampler_sm[2], (p - 0.5) / 9.0 + 0.5);
  float b = v.r;
  // コメントを外すとshadowmapを可視化する
  <%fragcolor/> = vec4(r, g, b, 1.0);
  // <%fragcolor/> = vec4(1.0);

  // nvidiaだとfoo()が0を返す。
  /*
  if (foo() == 0) {
    <%fragcolor/> = vec4(1.0);
  }
  */
}

