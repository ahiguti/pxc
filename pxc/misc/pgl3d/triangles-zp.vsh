
<%prepend/>
uniform mat4 view_projection_matrix;
<%decl_instance_attr mat4 model_matrix/>
<%vert_in/> vec3 position;
void main(void)
{
  vec4 gpos4 = <%instance_attr model_matrix/> * vec4(position, 1.0);
  gl_Position = view_projection_matrix * gpos4;
}

