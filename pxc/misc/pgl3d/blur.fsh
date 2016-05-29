
<%prepend/>
<%decl_fragcolor/>
<%frag_in/> vec2 vary_coord;
uniform vec2 pixel_delta;
uniform sampler2D sampler_tex;
uniform float option_value;

vec3 tex_read(in vec2 delta, in float k)
{
  vec3 v = <%texture2d/>(sampler_tex, vary_coord + delta * pixel_delta).xyz;
  return (v * v) * k;
}

void main(void)
{
  if (option_value != 0.0) {
    vec4 v = <%texture2d/>(sampler_tex, vary_coord);
    <%fragcolor/> = vec4(v.rgb, 1.0);
  } else {
    float p0 = <%blur_param/>; // 0.333 <= k0 <= 1.0
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
  }
}

