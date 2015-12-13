
<%prepend/>
uniform mat4 view_projection_matrix;
<%decl_instance_attr mat4 model_matrix/>
<%vert_in/> vec3 position;
<%if><%not><%eq><%opt/>0<%/><%/>
  <%vert_in/> vec3 uvw;
  <%vert_out/> vec3 vary_uvw;
<%/>
void main(void)
{
  vec4 gpos4 = <%instance_attr model_matrix/> * vec4(position, 1.0);
  gl_Position = view_projection_matrix * gpos4;
  <%if><%not><%eq><%opt/>0<%/><%/>
    vary_uvw = uvw;
  <%/>
}

