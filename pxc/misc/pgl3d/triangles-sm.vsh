
<%prepend/>
<%if><%light_fixed/>
  uniform vec3 trans;
  uniform float scale;
<%else/>
  uniform mat4 shadowmap_vp;
<%/>
<%decl_instance_attr mat4 model_matrix/>
<%vert_in/> vec3 position;
<%if><%not><%enable_depth_texture/><%/>
  <%vert_out/> vec4 vary_smpos;
<%/>
void main(void)
{
  vec4 p = shadowmap_vp * (<%instance_attr model_matrix/>
    * vec4(position, 1.0));
  p.z = max(p.z, -1.0);
    // nearより手前ならnearに。(これは正射影のときのみしか使えない。)
  gl_Position = p;
  <%if><%not><%enable_depth_texture/><%/>
    vary_smpos = p;
  <%/>
}
