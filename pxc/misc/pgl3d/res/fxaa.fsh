<%import>pre.fsh<%/>

<%decl_fragcolor/>
<%frag_in/> vec2 vary_coord;
uniform vec2 pixel_delta;
uniform sampler2D sampler_tex;
uniform float option_value;
void main(void)
{
  if (option_value != 0.0) {
    vec4 tval = <%texture2d/>(sampler_tex, vary_coord);
    <%fragcolor/> = vec4(tval.rgb, 1.0);
  } else {
    float span_max = 8.0;
    float reduce_mul = 1.0/8.0;
    float reduce_min = 1.0/128.0;
    vec3 rgb_nw = <%texture2d/>(sampler_tex, vary_coord +
      vec2(-1.0, -1.0) * pixel_delta).xyz;
    vec3 rgb_ne = <%texture2d/>(sampler_tex, vary_coord +
      vec2( 1.0, -1.0) * pixel_delta).xyz;
    vec3 rgb_sw = <%texture2d/>(sampler_tex, vary_coord +
      vec2(-1.0,  1.0) * pixel_delta).xyz;
    vec3 rgb_se = <%texture2d/>(sampler_tex, vary_coord +
      vec2( 1.0,  1.0) * pixel_delta).xyz;
    vec3 rgb_m  = <%texture2d/>(sampler_tex, vary_coord).xyz;
    vec3 l = vec3(0.299, 0.587, 0.114);
    float l_nw = dot(rgb_nw, l);
    float l_ne = dot(rgb_ne, l);
    float l_sw = dot(rgb_sw, l);
    float l_se = dot(rgb_se, l);
    float l_m  = dot(rgb_m,  l);
    float l_min = min(l_m, min(min(l_nw, l_ne), min(l_sw, l_se)));
    float l_max = max(l_m, max(max(l_nw, l_ne), max(l_sw, l_se)));
    vec2 dir;
    dir.x = -((l_nw + l_ne) - (l_sw + l_se));
    dir.y =  ((l_nw + l_sw) - (l_ne + l_se));
    float dir_reduce = max(
      (l_nw + l_ne + l_sw + l_se) * (0.25 * reduce_mul), reduce_min);
    float rcp_dmin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dir_reduce);
    dir = min(vec2(span_max, span_max),
      max(vec2(-span_max, -span_max), dir * rcp_dmin)) * pixel_delta;
    vec3 va = 0.5 * (
      <%texture2d/>(sampler_tex, vary_coord+dir*(1.0/3.0-0.5)).xyz +
      <%texture2d/>(sampler_tex, vary_coord+dir*(2.0/3.0-0.5)).xyz);
    vec3 vb = va * 0.5 + 0.25 * (
      <%texture2d/>(sampler_tex, vary_coord+dir*(0.0/3.0-0.5)).xyz +
      <%texture2d/>(sampler_tex, vary_coord+dir*(3.0/3.0-0.5)).xyz);
    float lb = dot(vb, l);
    vec4 color = vec4(0.0, 0.0, 0.0, 1.0);
    if ((lb < l_min) || (lb > l_max)) {
      color.xyz = va;
    } else {
      color.xyz = vb;
    }
    <%fragcolor/> = color;
  }
}

