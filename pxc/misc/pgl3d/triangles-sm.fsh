
<%if><%ne><%stype/>1<%/>
  // stype != 1
  <%if><%enable_depth_texture/>
    <%empty_shader_frag/>
  <%else/>
    <%prepend/>
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

  <%prepend/>

  const float epsilon = 1e-6;
  // const float epsilon = 1e-5;
  const float epsilon2 = 1e-4;

  const ivec3 tile3_size = <%tile3_size/>;
  const ivec3 pat3_size = <%pat3_size/>;
  const ivec3 pattex3_size = tile3_size * pat3_size;
  const ivec3 map3_size = <%map3_size/>;
  const ivec3 virt3_size = <%virt3_size/>;

  uniform vec3 light_dir;
  uniform <%mediump_sampler3d/> sampler_voxtpat;
  uniform <%mediump_sampler3d/> sampler_voxtmap;
  <%flat/> <%frag_in/> vec4 vary_aabb_or_tconv;
  <%flat/> <%frag_in/> vec3 vary_aabb_min;
  <%flat/> <%frag_in/> vec3 vary_aabb_max;
  <%flat/> <%frag_in/> mat4 vary_model_matrix;
  <%frag_in/> vec3 vary_position_local;

  bool pos2_inside_2(in vec2 pos, in vec2 mi, in vec2 mx)
  {
    return pos.x >= mi.x && pos.y >= mi.y &&
      pos.x < mx.x && pos.y < mx.y;
  }

  vec3 voxel_get_next(inout vec3 pos_f, in vec3 spmin,
    in vec3 spmax, in vec3 d, out vec3 npos)
  {
    vec3 r = vec3(0.0);
    <%if><%is_gl3_or_gles3/>
    vec3 dpos = mix(spmin, spmax, greaterThan(d, vec3(0.0)));
    <%else/>
    vec3 gtz = vec3(greaterThan(d, vec3(0.0)));
    vec3 dpos = <%mix>spmin<%>spmax<%>gtz<%/>;
    <%/>
    vec3 delta = dpos - pos_f;
    vec3 delta_div_d = delta / d;
    vec3 c0 = pos_f.yzx + d.yzx * delta_div_d;
    vec3 c1 = pos_f.zxy + d.zxy * delta_div_d;
    if (pos2_inside_2(vec2(c0.x, c1.x), spmin.yz, spmax.yz)) {
      r.x = d.x > 0.0 ? 1.0 : -1.0;
      npos = vec3(dpos.x, c0.x, c1.x);
    } else if (pos2_inside_2(vec2(c0.y, c1.y), spmin.zx, spmax.zx)) {
      r.y = d.y > 0.0 ? 1.0 : -1.0;
      npos = vec3(c1.y, dpos.y, c0.y);
    } else {
      r.z = d.z > 0.0 ? 1.0 : -1.0;
      npos = vec3(c0.z, c1.z, dpos.z);
    }
    return r;
  }

  vec3 div_rem(inout vec3 x, float y)
  {
    vec3 r = floor(x / y);
    x -= r * y;
    return r;
  }

  int tilemap_fetch(in vec3 pos, int tmap_mip, int tpat_mip)
  {
    // float distance_unit = distance_unit_tmap_mip;
    vec3 curpos_f = pos * virt3_size;
    vec3 curpos_i = div_rem(curpos_f, 1.0);
    vec3 curpos_t = floor(curpos_i / tile3_size);
    vec3 curpos_tr = curpos_i - curpos_t * tile3_size; // 0から15の整数
    <%if><%is_gl3_or_gles3/>
    vec4 value = texelFetch(sampler_voxtmap, ivec3(curpos_t) >> tmap_mip,
      tmap_mip);
    <%else/>
    vec4 value = <%texture3d/>(sampler_voxtmap, curpos_t / map3_size);
    <%/>
    int node_type = int(floor(value.a * 255.0 + 0.5));
    bool is_pat = (node_type == 1);
    if (is_pat) {
      vec3 curpos_tp = floor(value.rgb * 255.0 + 0.5) * tile3_size;
        // 16刻み4096迄
      // distance_unit = distance_unit_tpat_mip;
      <%if><%is_gl3_or_gles3/>
      value = texelFetch(sampler_voxtpat,
        ivec3(curpos_tp + curpos_tr) >> tpat_mip, tpat_mip);
      <%else/>
      value = <%texture3d/>(sampler_voxtpat,
        (curpos_tp + curpos_tr) / pattex3_size);
      <%/>
      // value = vec4(1.0, 1.0, 1.0, 1.0);
      // value.xyz = vec3(0.0);
      node_type = int(floor(value.a * 255.0 + 0.5));
    }
    return node_type;
  }

  int raycast_waffle(inout vec3 pos, inout vec3 fragpos, in vec3 ray,
    in vec3 mi, in vec3 mx)
  {
    float dist_max = length(pos - fragpos);
    float di = 2.0;
    float near = 65536;
    if (true) { // どっちが速い？
      vec3 d = (mx - mi) / di;
      vec3 ad = d / ray;
      bvec3 ad_nega = lessThan(ad, vec3(0.0));
      vec3 f = mi + d * (0.5 + vec3(ad_nega) * (di - 1.0)) + epsilon;
        // adが正ならmiから0.5, 負ならmxから0.5
      ad = abs(ad);
      vec3 a = (f - pos) / ray;
      for (float i = 0.0; i < di; i = i + 1.0, a.x = a.x + ad.x) {
        if (a.x > 0 && a.x < dist_max) {
          if (tilemap_fetch(pos + ray * a.x, 0, 0) != 0) {
            near = min(a.x, near);
            break;
          }
        }
      }
      for (float i = 0.0; i < di; i = i + 1.0, a.y = a.y + ad.y) {
        if (a.y > 0 && a.y < dist_max) {
          if (tilemap_fetch(pos + ray * a.y, 0, 0) != 0) {
            near = min(a.y, near);
            break;
          }
        }
      }
      for (float i = 0.0; i < di; i = i + 1.0, a.z = a.z + ad.z) {
        if (a.z > 0 && a.z < dist_max) {
          if (tilemap_fetch(pos + ray * a.z, 0, 0) != 0) {
            near = min(a.z, near);
            break;
          }
        }
      }
    } else {
      vec3 d = (mx - mi) / di;
      vec3 f = mi + d * 0.5  + epsilon;
      vec3 ad = d / ray;
      vec3 a = (f - pos) / ray;
      for (float i = 0.0; i < di; i = i + 1.0, a = a + ad) {
        if (a.x > 0 && a.x < dist_max) {
          if (tilemap_fetch(pos + ray * a.x, 0, 0) != 0) {
            near = min(a.x, near);
          }
        }
        if (a.y > 0 && a.y < dist_max) {
          if (tilemap_fetch(pos + ray * a.y, 0, 0) != 0) {
            near = min(a.y, near);
          }
        }
        if (a.z > 0 && a.z < dist_max) {
          if (tilemap_fetch(pos + ray * a.z, 0, 0) != 0) {
            near = min(a.z, near);
          }
        }
      }
    }
    if (near < 65535) {
      pos = pos + ray * near;
      return 1;
    }
    return -1;
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
    vec3 aabb_min = vary_aabb_min;
    vec3 aabb_max = vary_aabb_max;
    vec3 pos = fragpos;
    bool cam_inside_aabb = false;
    float epsi = epsilon * 3.0;
    {
      pos = clamp(pos, aabb_min + epsi, aabb_max - epsi);
      vec3 pos_n;
      voxel_get_next(pos, aabb_min, aabb_max, light_local, pos_n);
      pos = pos_n;
    }
    pos = clamp(pos, aabb_min + epsi, aabb_max - epsi);
    int hit = -1;
    hit = raycast_waffle(pos, fragpos, -light_local, aabb_min, aabb_max);
    if (hit < 0) {
      discard;
    }
  }
<%/>

