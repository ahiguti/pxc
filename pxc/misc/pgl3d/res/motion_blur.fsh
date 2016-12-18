<%import>pre.fsh<%/>

<%decl_fragcolor/>
<%frag_in/> vec2 vary_coord;
uniform vec2 pixel_delta; // unused
uniform sampler2D sampler_tex[<%mbsz/>];
uniform float option_value;

void main(void)
{
  vec3 v = vec3(0.0);
  for (int i = 0; i < <%mbsz/>; ++i) {
    v = v + <%texture2d/>(sampler_tex[i], vary_coord).xyz;
  }
  v /= <%mbsz/>;
  <%fragcolor/> = vec4(v.rgb, 1.0);
}

