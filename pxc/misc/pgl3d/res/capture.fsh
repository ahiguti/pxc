<%import>pre.fsh<%/>

<%decl_fragcolor/>
<%frag_in/> vec2 vary_coord;
uniform sampler2D sampler_tex;
uniform vec2 pixel_delta;
uniform vec2 framebuffer_size;

vec3 tex_read(in vec2 delta)
{
  vec2 crd = vec2(vary_coord.x, 1.0 - vary_coord.y);
  return <%texture2d/>(sampler_tex, crd + delta * pixel_delta).rgb;
}

void main(void)
{
  vec2 crd = vec2(vary_coord.x, 1.0 - vary_coord.y);
  vec2 coord = floor(crd * framebuffer_size);
  vec2 coord_div2 = floor(coord * 0.5);
  vec2 coord_rem2 = coord - coord_div2 * 2.0;
  // if (coord_rem2.x > 0.5 || coord_rem2.y > 0.5) {
  vec3 v0 = <%texture2d/>(sampler_tex, crd).rgb;
  vec3 v1 = tex_read(vec2(1.0 - coord_rem2.x * 2.0, 0.0));
  vec3 vm = (v0 + v1) * 0.5;
  float y = 0.299 * v0.r + 0.587 * v0.g + 0.114 * v0.b; // Y
  float c;
  if (coord_rem2.x < 0.5) {
    // c = -0.168736 * vm.r - 0.331264 * vm.g + 0.5 * vm.b; // Cb
    c = -0.14713 * vm.r - 0.28886 * vm.g + 0.436 * vm.b; // U
  } else {
    // c = 0.5 * vm.r - 0.418688 * vm.g - 0.081312 * vm.b; // Cr
    c = 0.615 * vm.r - 0.51499 * vm.g - 0.10001 * vm.b; // Cr
  }
  <%fragcolor/> = vec4(y, c + 0.5, v0.r, 1.0);
  /*
  if (coord_rem2.x > 0.5 || coord_rem2.y > 0.5) {
    vec4 v = <%texture2d/>(sampler_tex, vary_coord);
    <%fragcolor/> = vec4(v.rgb, 1.0);
  } else {
    <%fragcolor/> = vec4(1.0, 1.0, 1.0, 1.0);
  }
  */
}

