
<%prepend/>
<%if><%light_fixed/>
  uniform vec3 trans;
  uniform float scale;
<%else/>
  uniform mat4 shadowmap_vp;
<%/>
<%decl_instance_attr mat4 model_matrix/>
<%vert_in/> vec3 position;
<%if><%not><%eq><%opt/>0<%/><%/>
  <%vert_in/> vec3 uvw;
  <%vert_out/> vec3 vary_uvw;
<%/>
<%if><%not><%enable_depth_texture/><%/>
  <%vert_out/> vec4 vary_smpos;
<%/>
void main(void)
{
  <%if><%light_fixed/>
    vec4 p = <%instance_attr model_matrix/> * vec4(position, 1.0);
    vec3 p1 = p.xyz + trans;
    p1 *= scale
    p = vec4(p1, 1.0);
    gl_Position = p;
  <%else/>
    vec4 p = shadowmap_vp * (<%instance_attr model_matrix/>
      * vec4(position, 1.0));
    gl_Position = p;
  <%/>
  <%if><%not><%enable_depth_texture/><%/>
    vary_smpos = p;
  <%/>
  <%if><%not><%eq><%opt/>0<%/><%/>
    vary_uvw = uvw;
  <%/>
}
