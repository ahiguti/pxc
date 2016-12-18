<%import>pre.vsh<%/>

<%if><%light_fixed/>
  uniform vec3 trans;
  uniform float scale;
<%else/>
  uniform mat4 shadowmap_vp;
<%/>
uniform vec3 light_dir; // unused?
uniform vec3 camera_pos;
<%decl_instance_attr mat4 model_matrix/>
<%vert_in/> vec3 position;
  // stype==0のとき頂点のオブジェクト座標
  // stype==1のときオブエジェクト座標系での接線空間の原点
<%if><%eq><%stype/>1<%/>
  <%vert_in/> vec3 normal;   // オブジェクト座標系での法線(接線空間のz軸)
  <%vert_in/> vec3 tangent;  // オブジェクト座標系での接線(接線空間のx軸)
  <%vert_in/> vec3 uvw;      // 頂点のテクスチャ座標
  <%vert_in/> vec4 aabb_or_tconv;
    // stype==0のときテクスチャ座標の範囲aabb
    // stype==1のときテクスチャ座標から接線空間への変換
  <%vert_in/> vec3 aabb_min; // テクスチャ座標の範囲aabb
  <%vert_in/> vec3 aabb_max; // テクスチャ座標の範囲aabb
  <%flat/> <%vert_out/> vec4 vary_aabb_or_tconv; // aabb_or_tconvと同じ
  <%flat/> <%vert_out/> vec3 vary_aabb_min;
  <%flat/> <%vert_out/> vec3 vary_aabb_max;
  <%flat/> <%vert_out/> mat4 vary_model_matrix; // 接線空間からワールドへの変換
  <%vert_out/> vec3 vary_position_local; // 接線空間での頂点座標
  <%flat/> <%vert_out/> vec3 vary_camerapos_local; // 接線空間でのカメラ座標
  <%flat/> <%vert_out/> mat4 vary_mvp;
<%/>
<%if><%not><%enable_depth_texture/><%/>
  <%vert_out/> vec4 vary_smpos;
<%/>
void main(void)
{
  mat4 mm = <%instance_attr model_matrix/>;
  <%if><%ne><%stype/>1<%/>
    // opt==0 parallax mapping
    vec4 gpos4 = mm * vec4(position, 1.0);
    vec4 p = shadowmap_vp * gpos4;
    p.z = max(p.z, -1.0);
      // nearより手前ならnearに。(これは正射影のときのみしか使えない。)
  <%else/>
    // opt==1 voxel raycasting
    // このケースではpositionはオブジェクト座標系での接線空間の原点位置を表す
    vary_position_local = aabb_or_tconv.xyz + uvw * aabb_or_tconv.w;
      // 接線空間での頂点座標をテクスチャ座標から計算
    vec3 binormal = cross(normal, tangent);
      // オブジェクト座標系での接線空間のy軸
    mat4 tan_to_obj = mat4(
      vec4(tangent, 0.0),
      vec4(binormal, 0.0),
      vec4(normal, 0.0),
      vec4(position, 1.0)); // 接線空間からオブジェクト座標系への変換
    mm = mm * tan_to_obj; // 接線空間からワールド座標への変換
    <%if><%is_gl3_or_gles3/>
      mat3 normal_matrix = mat3(mm);
    <%else/>
      mat3 normal_matrix = mat3(mm[0].xyz, mm[1].xyz, mm[2].xyz);
    <%/>
    vec3 model_transform = mm[3].xyz;
    vec4 gpos4 = mm * vec4(vary_position_local, 1.0);
      // ワールドでの頂点の座標
    vary_model_matrix = mm; // 接線空間からワールドへの変換
    vary_camerapos_local = (camera_pos - model_transform) * normal_matrix;
      // mmは等倍回転と加算だけなので右辺は inverse(mm) * camera_posに等しい
    vary_aabb_or_tconv = aabb_or_tconv;
    vary_aabb_min = aabb_min;
    vary_aabb_max = aabb_max;
    vary_mvp = shadowmap_vp * mm;
    vec4 p = shadowmap_vp * gpos4;
    p.z = 0.0;
      // デプス値はfsで計算する。clipされないように0にしておく。
      // (正射影を前提)
  <%/>
  gl_Position = p;
  <%if><%not><%enable_depth_texture/><%/>
    vary_smpos = p;
  <%/>
}
