
<%prepend/>
uniform mat4 view_projection_matrix;
uniform vec3 camera_pos;
<%if><%light_fixed/>
  // uniform vec3 camera_pos;
<%else/>
  uniform mat4 shadowmap_vp[<%smsz/>];
<%/>
<%decl_instance_attr mat4 model_matrix/>
<%vert_in/> vec3 position;
<%vert_in/> vec3 normal;
<%vert_in/> vec3 tangent;
<%vert_in/> vec3 uvw;
<%vert_in/> vec4 uv_aabb;
<%vert_out/> vec3 vary_position;
<%vert_out/> vec3 vary_normal;
<%vert_out/> vec3 vary_tangent;
<%vert_out/> vec3 vary_binormal;
<%vert_out/> vec3 vary_uvw;
<%vert_out/> vec4 vary_uv_aabb;
<%if><%enable_shadowmapping/>
  <%if><%not><%light_fixed/><%/>
    <%vert_out/> vec3 vary_smposa[<%smsz/>];
    uniform float ndelta_scale; // 0.02
  <%/>
<%/>
<%if><%not><%eq><%opt/>0<%/><%/>
<%vert_out/> mat4 vary_model_matrix;
<%vert_out/> vec3 vary_position_local;
<%vert_out/> vec3 vary_camerapos_local;
//  <%vert_out/> vec3 vary_camera_local;
//  <%vert_out/> vec3 vary_light_local;
<%/>
void main(void)
{
  mat4 mm = <%instance_attr model_matrix/>;
  vec4 gpos4 = mm * vec4(position, 1.0);
  gl_Position = view_projection_matrix * gpos4;
  <%if><%is_gl3_or_gles3/>
    mat3 normal_matrix = mat3(mm);
  <%else/>
    mat3 normal_matrix = mat3(mm[0].xyz, mm[1].xyz, mm[2].xyz);
  <%/>
  vary_position = gpos4.xyz / gpos4.w;
  vary_normal = normal_matrix * normal;
  vary_tangent = normal_matrix * tangent;
  <%if><%not><%eq><%opt/>0<%/><%/>
    vary_uvw = position; // FIXME
  <%else/>
    vary_uvw = uvw;
  <%/>
  vary_binormal = cross(vary_normal, vary_tangent);
  vary_uv_aabb = uv_aabb;
  <%if><%enable_shadowmapping/>
    <%if><%not><%light_fixed/><%/>
      vec3 ndelta = mat3(shadowmap_vp[0]) * vary_normal * ndelta_scale; // 0.02
      vec4 p;
      vec4 ngpos4 = vec4(vary_position, 1.0);
      <%variable d>
	<%set d 1/>
	<%for i 0><%smsz/>
	  p = shadowmap_vp[<%i/>] * ngpos4;
	  vary_smposa[<%i/>] = p.xyz / p.w + ndelta / <%d/>.;
	  <%set d><%mul><%d/>3<%/><%/>
	<%/>
      <%/>
    <%/>
  <%/>
  <%if><%not><%eq><%opt/>0<%/><%/>
    vary_model_matrix = mm;
    vary_position_local = position;
    vec4 cp4 = inverse(mm) * vec4(camera_pos, 1.0); // FIXME inverse
    vary_camerapos_local = cp4.xyz / cp4.w;
//    vary_camera_local = camera * normal_matrix;
//    vary_light_local = light * normal_matrix;
  <%/>
}
