
<%prepend/>
uniform mat4 view_projection_matrix;
<%decl_instance_attr mat4 model_matrix/>
<%vert_in/> vec3 position;
void main(void)
{
  vec4 gpos4 = <%instance_attr model_matrix/> * vec4(position, 1.0);
  vec4 pos = view_projection_matrix * gpos4;
  <%if><%eq><%stype/>2<%/>
  // stype 2はボクセルマップの底に表示するzprepassとshadowのみのポリゴン
  // TODO: バイアスはデプスバッファのビット数に合わせて決める?
  // gl_Position = pos + vec4(0.0, 0.0, 0.0001 * pos.w, 0.0);
  gl_Position = pos + vec4(0.0, 0.0, 0.0001 * pos.w, 0.0);
  <%else/>
  gl_Position = pos;
  <%/>
}

