<%import>pre.fsh<%/>
<%import>triangles-inc.fsh<%/>

uniform vec3 camera_pos;
<%if><%ne><%stype/>1<%/>
  <%empty_shader_frag/>
<%else/>
  // stype == 1 raycasting
  <%if><%eq><%debug_zprepass/>1<%/>
    <%decl_fragcolor/>
  <%/>

  <%flat/> <%frag_in/> vec4 vary_aabb_or_tconv;
  <%flat/> <%frag_in/> vec3 vary_aabb_min;
  <%flat/> <%frag_in/> vec3 vary_aabb_max;
  <%flat/> <%frag_in/> mat4 vary_model_matrix;
  <%frag_in/> vec3 vary_position_local;
  <%flat/> <%frag_in/> vec3 vary_camerapos_local;
  <%frag_in/> vec3 vary_position;

  void main(void)
  {
    <%if><%is_gl3_or_gles3/>
      mat3 normal_matrix = mat3(vary_model_matrix);
    <%else/>
      mat4 mm = vary_model_matrix;
      mat3 normal_matrix = mat3(mm[0].xyz, mm[1].xyz, mm[2].xyz);
    <%/>
    vec3 rel_camera_pos = camera_pos - vary_position;
    vec3 camera_dir = normalize(rel_camera_pos);
    vec3 camera_local = -camera_dir * normal_matrix;
      // カメラから物体への向き、接線空間
    float texscale = 1.0 / vary_aabb_or_tconv.w;
    vec3 texpos = - vary_aabb_or_tconv.xyz * texscale;
      // texpos,texscaleは接線空間からテクスチャ座標への変換のパラメータ
    vec3 fragpos = texpos + vary_position_local * texscale;
      // テクスチャ座標でのフラグメント位置
    vec3 campos = texpos + vary_camerapos_local * texscale;
      // テクスチャ座標でのカメラ位置
    vec3 aabb_min = vary_aabb_min;
    vec3 aabb_max = vary_aabb_max;
    vec3 pos = fragpos;
    bool cam_inside_aabb = false;
    <%if><%raycast_cull_front/>
      // カメラが直方体の内側に入ってもレンダリングできるように
      // する処理。Frontカリングを有効にしていることを前提。
      float epsi = epsilon * 3.0;
      cam_inside_aabb = pos3_inside_3_ge_le(campos, aabb_min, aabb_max);
      if (cam_inside_aabb) {
        // カメラは直方体の内側にある
        pos = campos;
        // <%fragcolor/> = vec4(1.0,1.0,0.0,1.0); return;
      } else {
        // カメラは直方体の外側にある
        pos = clamp(pos, aabb_min + epsi, aabb_max - epsi);
        vec3 pos_n;
        voxel_get_next(pos, aabb_min, aabb_max, -camera_local, pos_n);
        pos = pos_n;
      }
      pos = clamp(pos, aabb_min + epsi, aabb_max - epsi);
    <%/>
    int miplevel = clamp(raycast_get_miplevel(pos, campos, 0.0), 0, 8);
    // miplevel = 0;
    int hit = -1;
    hit = raycast_waffle(pos, fragpos, camera_local, aabb_min, aabb_max,
      miplevel);
    if (hit < 0) {
      discard;
    }
    <%if><%eq><%debug_zprepass/>1<%/>
      <%fragcolor/> = vec4(1.0, 1.0, 0.0, 1.0);
    <%/>
  }

<%/>
