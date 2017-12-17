<%import>pre.fsh<%/>

<%decl_fragcolor/>
<%frag_in/> vec2 vary_coord;
uniform sampler2D sampler_tex;

vec4 get_tex(in vec2 coord)
{
  vec2 c = (coord + 1.0) * 0.5;
  return <%texture2d/>(sampler_tex, c);
}

const float delta = 0.002;
const float scale_r = 1.0 + delta;
const float scale_g = 1.0;
const float scale_b = 1.0 - delta;

void main(void)
{
  vec2 coord = vary_coord * 2.0 - 1.0;
  float vr = get_tex(coord * scale_r).r;
  float vg = get_tex(coord * scale_g).g;
  float vb = get_tex(coord * scale_b).b;
  <%fragcolor/> = vec4(vr, vg, vb, 1.0);
}

