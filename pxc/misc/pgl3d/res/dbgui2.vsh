<%import>pre.vsh<%/>

<%vert_in/> vec2 vert;
<%vert_out/> vec2 vary_vert;
<%decl_instance_attr vec4 idata/>
void main(void)
{
  vec4 idata_i = <%instance_attr idata/>;
  vec2 screen_pos = idata_i.xy + idata_i.zw * vert;
  gl_Position = vec4(screen_pos, 0.0, 1.0);
  vary_vert = vert;
}
