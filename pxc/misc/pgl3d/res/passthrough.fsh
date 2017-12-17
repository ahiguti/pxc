<%import>pre.fsh<%/>

<%decl_fragcolor/>
<%frag_in/> vec2 vary_coord;
uniform sampler2D sampler_tex;

void main(void)
{
  vec4 v = <%texture2d/>(sampler_tex, vary_coord);
  <%fragcolor/> = vec4(v.rgb, 1.0);
}

