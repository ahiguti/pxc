
<%prepend/>
uniform mat4 view_projection_matrix;
uniform vec3 camera_pos;
<%if><%light_fixed/>
  // uniform vec3 camera_pos;
<%else/>
  uniform mat4 shadowmap_vp[<%smsz/>];
<%/>
<%decl_instance_attr mat4 model_matrix/>
<%vert_in/> vec3 position; // opt==0のとき頂点のオブジェクト座標
                           // opt==1のときオブエジェクト座標系での接空間の原点
<%vert_in/> vec3 normal;   // オブジェクト座標系での法線(接空間のz軸)
<%vert_in/> vec3 tangent;  // オブジェクト座標系での接線(接空間のx軸)
<%vert_in/> vec3 uvw;      // 頂点のテクスチャ座標
<%vert_in/> vec4 aabb_or_tconv; // opt==0のときテクスチャ座標の範囲aabb
                                // opt==1のときテクスチャ座標から接空間への変換
<%if><%not><%eq><%opt/>0<%/><%/>
  <%vert_in/> vec3 aabb_min; // テクスチャ座標の範囲aabb
  <%vert_in/> vec3 aabb_max; // テクスチャ座標の範囲aabb
<%/>
<%vert_out/> vec3 vary_position; // ワールドの頂点座標
<%vert_out/> vec3 vary_normal;   // ワールドでの法線
<%vert_out/> vec3 vary_tangent;  // ワールドでの接線
<%vert_out/> vec3 vary_binormal; // ワールドでの従法線
<%vert_out/> vec3 vary_uvw;      // 頂点のテクスチャ座標
<%vert_out/> vec4 vary_aabb_or_tconv; // aabb_or_tconvと同じ
<%if><%not><%eq><%opt/>0<%/><%/>
  <%vert_out/> vec3 vary_aabb_min;
  <%vert_out/> vec3 vary_aabb_max;
  <%vert_out/> mat4 vary_model_matrix;    // 接空間からワールドへの変換
  <%vert_out/> vec3 vary_position_local;  // 接空間での頂点座標
  <%vert_out/> vec3 vary_camerapos_local; // 接空間でのカメラ座標
<%/>
<%if><%enable_shadowmapping/>
  <%if><%not><%light_fixed/><%/>
    <%vert_out/> vec3 vary_smposa[<%smsz/>];
    uniform float ndelta_scale; // 0.02
  <%/>
<%/>
void main(void)
{
  mat4 mm = <%instance_attr model_matrix/>;
  <%if><%eq><%opt/>0<%/>
    // opt==0 parallax mapping
    <%if><%is_gl3_or_gles3/>
      mat3 normal_matrix = mat3(mm);
    <%else/>
      mat3 normal_matrix = mat3(mm[0].xyz, mm[1].xyz, mm[2].xyz);
    <%/>
    vec4 gpos4 = mm * vec4(position, 1.0);
    gl_Position = view_projection_matrix * gpos4;
    vary_position = gpos4.xyz / gpos4.w;
    vary_normal = normal_matrix * normal;
    vary_tangent = normal_matrix * tangent;
    vary_uvw = uvw;
    vary_binormal = cross(vary_normal, vary_tangent);
    vary_aabb_or_tconv = aabb_or_tconv;
  <%else/>
    // opt==1 voxel raycasting
    // このケースではpositionはオブジェクト座標系での接空間の原点位置を表す
    vary_position_local = aabb_or_tconv.xyz + uvw * aabb_or_tconv.w;
      // 接空間での頂点座標をテクスチャ座標から計算
    vec3 binormal = cross(normal, tangent);
      // オブジェクト座標系での接空間のy軸
    mat4 tan_to_obj = mat4(
      vec4(tangent, 0.0),
      vec4(binormal, 0.0),
      vec4(normal, 0.0),
      vec4(position, 1.0)); // 接空間からオブジェクト座標系への変換
    mat4 obj_to_tan = inverse(tan_to_obj);
    mm = mm * tan_to_obj; // 接空間からワールド座標への変換
    <%if><%is_gl3_or_gles3/>
      mat3 normal_matrix = mat3(mm);
    <%else/>
      mat3 normal_matrix = mat3(mm[0].xyz, mm[1].xyz, mm[2].xyz);
    <%/>
    vec4 gpos4 = mm * vec4(vary_position_local, 1.0);
      // ワールドでの頂点の座標
    gl_Position = view_projection_matrix * gpos4;
    vary_position = gpos4.xyz / gpos4.w; // ワールドでの頂点の座標
    vary_normal = normal_matrix * normal;
    vary_tangent = normal_matrix * tangent;
    vary_binormal = normal_matrix * tangent;
    vary_uvw = uvw;
    vary_model_matrix = mm; // 接空間からワールドへの変換
    vec4 cp4 = inverse(mm) * vec4(camera_pos, 1.0);
    vary_camerapos_local = cp4.xyz / cp4.w; // 接空間でのカメラ位置
    vary_aabb_or_tconv = aabb_or_tconv;
    vary_aabb_min = aabb_min;
    vary_aabb_max = aabb_max;
  <%/>
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
//  <%if><%not><%eq><%opt/>0<%/><%/>
//    vary_model_matrix = mm;
//    vary_position_local = position;
//    vec4 cp4 = inverse(mm) * vec4(camera_pos, 1.0); // FIXME inverse
//    vary_camerapos_local = cp4.xyz / cp4.w;
//    vary_aabb_or_tconv = vec4(-32.0, -32.0, -32.0, 64.0);
//  <%/>
}

