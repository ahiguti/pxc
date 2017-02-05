<%import>pre.fsh<%/>
<%import>triangles-inc.fsh<%/>

uniform mat4 shadowmap_vp;

<%if><%ne><%stype/>1<%/>
  // stype != 1
  <%if><%enable_depth_texture/>
    <%empty_shader_frag/>
  <%else/>
    <%if><%not><%enable_depth_texture/><%/>
      <%frag_in/> vec4 vary_smpos;
      <%decl_fragcolor/>
    <%/>
    void main(void)
    {
      <%if><%not><%enable_depth_texture/><%/>
	<%if><%enable_vsm/>
	  vec4 p = vary_smpos;
	  float pz = (p.z/p.w + 1.0) / 2.0; // TODO: calc in vsh
	  <%fragcolor/> = vec4(pz, pz * pz, 0.0, 0.0);
	<%else/>
	  <%if><%not><%enable_depth_texture/><%/>
	    vec4 p = vary_smpos;
	    float pz = (p.z/p.w + 1.0) / 2.0; // TODO: calc in vsh
	    // TODO: vectorize
	    float z = pz * 256.0; // [0.0, 256.0]
	    float z0 = floor(z); // [0, 256]
	    z = (z - z0) * 256.0; // [0.0, 256.0)
	    float z1 = floor(z); // [0, 256)
	    z = (z - z1) * 256.0; // [0.0, 256.0)
	    float z2 = floor(z);  // [0, 256)
	    <%fragcolor/> = vec4(z0/255.0, z1/255.0, z2/255.0, 1.0);
	  <%/>
	<%/>
      <%/>
    }
  <%/>
<%else/>
  // stype == 1 raycasting
  uniform vec3 light_dir;
  <%flat/> <%frag_in/> vec4 vary_aabb_or_tconv;
  <%flat/> <%frag_in/> vec3 vary_aabb_min;
  <%flat/> <%frag_in/> vec3 vary_aabb_max;
  <%flat/> <%frag_in/> mat4 vary_model_matrix;
  <%frag_in/> vec3 vary_position_local;
  <%flat/> <%frag_in/> vec3 vary_camerapos_local;
  <%flat/> <%frag_in/> mat4 vary_mvp;

  float generate_random(vec3 v)
  {
    return fract(sin(dot(v.xy, vec2(12.9898, 78.233))) * 43758.5453);
  }

  void main(void)
  {
    <%if><%is_gl3_or_gles3/>
      mat3 normal_matrix = mat3(vary_model_matrix);
    <%else/>
      mat4 mm = vary_model_matrix;
      mat3 normal_matrix = mat3(mm[0].xyz, mm[1].xyz, mm[2].xyz);
    <%/>
    vec3 light_local = light_dir * normal_matrix;
      // 光源への向き
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
    float epsi = epsilon * 3.0;
    {
      pos = clamp(pos, aabb_min + epsi, aabb_max - epsi);
      vec3 pos_n;
      voxel_get_next(pos, aabb_min, aabb_max, light_local, pos_n);
      pos = pos_n;
    }
    pos = clamp(pos, aabb_min + epsi, aabb_max - epsi);
    float dist_rnd = generate_random(pos) * 0.5;
    int miplevel = clamp(raycast_get_miplevel(pos, campos, dist_rnd), 0, 8);
    miplevel = min(miplevel, 5);
    int hit = -1;
    hit = raycast_waffle(pos, fragpos, -light_local, aabb_min, aabb_max,
      miplevel);
    if (hit < 0) {
      discard;
    }
    // TODO: depthの計算はもっと速くできる
    pos = vary_aabb_or_tconv.xyz + pos * vary_aabb_or_tconv.w;
      // posをテクスチャ座標から接線空間の座標に変換
    /*
    vec4 gpos = vary_model_matrix * vec4(pos, 1.0);
      // vary_model_matrixは接線空間からワールドへの変換
    vec4 vpos = shadowmap_vp * gpos;
    */
    /*
    vec4 vpos = vary_mvp * vec4(pos, 1.0);
    // float depth = (vpos.z / vpos.w + 1.0) * 0.5;
    float depth = (vpos.z + 1.0) * 0.5;
    */
    float vposz = dot(
      vec4(vary_mvp[0][2], vary_mvp[1][2], vary_mvp[2][2], vary_mvp[3][2]),
      vec4(pos, 1.0));
    float depth = (vposz + 1.0) * 0.5;
    depth = clamp(depth, 0.01, 0.99);
    gl_FragDepth = depth;
  }
<%/>

