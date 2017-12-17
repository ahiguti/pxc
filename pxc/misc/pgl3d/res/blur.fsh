<%import>pre.fsh<%/>

<%decl_fragcolor/>
<%frag_in/> vec2 vary_coord;
uniform vec2 pixel_delta;
uniform sampler2D sampler_tex;
uniform sampler2D sampler_tex_depth;
uniform float option_value;

vec3 tex_read(in vec2 delta, in float k)
{
  vec3 v = <%texture2d/>(sampler_tex, vary_coord + delta * pixel_delta).xyz;
  return (v * v) * k;
}

const float weight[25] = float[25](
  1., 4., 7., 4., 1.,
  4., 16., 26., 16., 4.,
  7., 26., 41., 26., 7.,
  4., 16., 26., 16., 4.,
  1., 4., 7., 4., 1.
);

void main(void)
{
  if (option_value != 0.0) {
    vec4 v = <%texture2d/>(sampler_tex, vary_coord);
    <%fragcolor/> = vec4(v.rgb, 1.0);
  /*
  } else {
    vec3 v = vec3(0.0);
    for (int fy = 0; fy < 5; ++fy) {
      for (int fx = 0; fx < 5; ++fx) {
        float w = weight[fy * 5 + fx];
        v += tex_read(vec2(float(fx) - 2.0, float(fy) - 2.0), w);
      }
    }
    v *= 1.0 / 273.0;
    <%fragcolor/> = vec4(sqrt(v), 1.0);
  */
  } else {
    float p0;
    <%if><%enable_bokeh/>
      vec4 dep = <%texture2d/>(sampler_tex_depth, vary_coord);
      float depval = dep.x;
      p0 = 1.0 - max((depval - 0.995) * 200.0, 0.0) * 0.5;
    <%else/>
      p0 = <%blur_param/>; // 0.333 <= k0 <= 1.0
    <%/>
    float p1 = (1.0 - p0) * 0.5;
    float k0 = p0 * p0;
    float k1 = p0 * p1;
    float k2 = p1 * p1;
    vec3 v =
      tex_read(vec2( 0.0,  0.0), k0) +
      tex_read(vec2(-1.0,  0.0), k1) +
      tex_read(vec2( 1.0,  0.0), k1) +
      tex_read(vec2( 0.0, -1.0), k1) +
      tex_read(vec2( 0.0,  1.0), k1) +
      tex_read(vec2(-1.0, -1.0), k2) +
      tex_read(vec2( 1.0, -1.0), k2) +
      tex_read(vec2(-1.0,  1.0), k2) +
      tex_read(vec2( 1.0,  1.0), k2);
    <%fragcolor/> = vec4(sqrt(v), 1.0);
  /*
  */
  }
}

