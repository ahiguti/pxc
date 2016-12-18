<%import>pre.fsh<%/>

<%decl_fragcolor/>
<%frag_in/> vec2 vary_vert;
uniform sampler2D sampler_sm[<%smsz/>];
void main(void)
{
  vec4 v = <%texture2d/>(sampler_sm[0], vary_vert);
  float r = v.r;
  v = <%texture2d/>(sampler_sm[1], (vary_vert - 0.5) / 3.0 + 0.5);
  float g = v.r;
  // <%fragcolor/> = vec4(float(r < 0.9), float(g < 0.9), 0.0, 1.0);
}
