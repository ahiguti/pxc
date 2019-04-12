<%import>pre.fsh<%/>
<%import>triangles-inc.fsh<%/>
<%import>fnoise.fsh<%/>

// #pragma optionNV(inline all)
// #pragma optionNV(unroll none)
// #pragma optionNV(fastmath off)
// #pragma optionNV(fastprecision off)
// #pragma optionNV(strict off)

// #extension GL_ARB_conservative_depth : enable
// <%if><%eq><%stype/>1<%/>
// layout (depth_unchanged) out float gl_FragDepth;
// <%else/>
// layout (depth_unchanged) out float gl_FragDepth;
// <%/>

const float tile_size = 64.0;
const float tilemap_size = 128.0;
const float tiletex_size = 1024.0;
uniform sampler2D sampler_dpat;
uniform sampler2D sampler_pmpat;
uniform sampler2D sampler_tilemap;
uniform samplerCube sampler_env;
uniform sampler2D sampler_noise;
<%if><%enable_sampler2dshadow/>
  uniform sampler2DShadow sampler_sm[<%smsz/>];
<%else/>
  uniform sampler2D sampler_sm[<%smsz/>];
<%/>
<%if><%eq><%stype/>1<%/>
  uniform sampler2D sampler_depth_rd;
<%/>
uniform mat4 view_projection_matrix;
uniform vec3 camera_pos;
uniform mat4 shadowmap_vp[<%smsz/>];
uniform float ndelta_scale; // 0.02
uniform vec3 light_dir;
uniform float light_on;
uniform float option_value;
uniform float option_value2;
uniform float exposure;
/*
uniform float random_seed;
*/
<%frag_in/> vec3 vary_position;
<%frag_in/> vec3 vary_normal;
<%frag_in/> vec3 vary_tangent;
<%frag_in/> vec3 vary_binormal;
<%if><%eq><%stype/>1<%/>
  <%flat/> <%frag_in/> vec4 vary_aabb_or_tconv;
  <%flat/> <%frag_in/> vec3 vary_aabb_min;
  <%flat/> <%frag_in/> vec3 vary_aabb_max;
  <%flat/> <%frag_in/> mat4 vary_model_matrix;
  <%frag_in/> vec3 vary_position_local;
  <%flat/> <%frag_in/> vec3 vary_camerapos_local;
<%else/>
  <%frag_in/> vec3 vary_uvw;
  <%frag_in/> vec4 vary_aabb_or_tconv;
<%/>
<%decl_fragcolor/>

const float shadowmap_max_distance = <%shadowmap_max_distance/>;

<%if><%enable_shadowmapping/>
  <%if><%ne><%stype/>1<%/>
    <%frag_in/> vec3 vary_smposa[<%smsz/>];
  <%/>
<%/>

/*
float generate_random(vec3 v)
{
  v.x += fract(random_seed);
  v.y += fract(random_seed);
  return fract(sin(dot(v.xy, vec2(12.9898, 78.233))) * 43758.5453);
}
*/

bool uv_inside_aabb(in vec2 uv)
{
  vec4 aabb = floor(vary_aabb_or_tconv + 0.5);
  return uv.x >= aabb.x && uv.y >= aabb.y && uv.x < aabb.z && uv.y < aabb.w;
}

vec3 clamp_to_border(in vec2 uv, in vec3 delta)
{
  vec4 aabb = floor(vary_aabb_or_tconv + 0.5);
  vec2 dclamp = clamp(delta.xy, aabb.xy - uv + 0.5, aabb.zw - uv - 0.5);
	  ///// FIXME FIXME FIXME
  float ratx = (abs(delta.x) > 0.001) ? (dclamp.x / delta.x) : 0.0;
  float raty = (abs(delta.y) > 0.001) ? (dclamp.y / delta.y) : 0.0;
  float rat = clamp(min(ratx, raty), 0.0, 1.0);
    // vec2 rat2 = dclamp / delta.xy;
    // float rat = min(0.99, min(rat2.x, rat2.y));
  vec3 r = delta * rat;
  vec2 np = uv + r.xy;   /// HAMIDASU
    // vec2 np = uv + dclamp;  /// HAMIDASANAI
    //////// vec2 np = uv + clamp(delta.xy, vec2(0.0,0.0), aabb.zw -uv) * 0.99;
    //////// vec2 np = uv + clamp(delta.xy, aabb.xy - uv, aabb.zw -uv) * 0.99;
    // if (np.x >= aabb.z || np.y >= aabb.w) { dbgval.r = 1.0; }
    // if (ratx < 0 || raty < 0) { dbgval.b = 1.0; }
    // if (!uv_inside_aabb(np)) { dbgval.r = 1.0; }
    // if (np.x < aabb.x || np.y < aabb.y ||
    //    np.x >= aabb.z || np.y >= aabb.w) { dbgval.r = 1.0; }
  return r;
}

<%if><%enable_normalmapping/>

  void tilemap(in vec2 uv0, out vec4 tex_val1, out vec2 subtex_uv)
  {
    vec2 uv_tm = floor(uv0 / tile_size);
	    // tilemap coordinate
    vec2 uv_tmfr = uv0 / tile_size - uv_tm;
	    // coordinate inside a tile (0, 1)
    vec2 uv_ti = uv_tmfr * tile_size;
    vec2 uvi = floor(uv_ti);
	    // coordinate inside a tile, integral
    subtex_uv = uv_ti - uvi;
	    // subtexel coordinate
    vec4 ti_val = <%texture2d/>
      (sampler_tilemap, uv_tm / tilemap_size);
	    // lookup the tilemap
    vec2 uv_pixel = floor(ti_val.xy * 255.0 + 0.5) * tile_size + uvi;
	    // tile pattern coordinate
    if (uv_inside_aabb(uv0)) {
      tex_val1 = <%texture2d/>(sampler_dpat, uv_pixel / tiletex_size);
	    // lookup the tilepattern
    } else {
      tex_val1 = vec4(0.5, 0.5, 0.5, 0.0);
    }
  }

<%/> // enable_normalmapping

<%if><%enable_parallax/>

  void parallax_read(in vec2 uv0, out vec4 tex_val1)
  {
    vec2 uv_tm = floor(uv0 / tile_size); // tilemap coordinate
    vec2 uv_tmfr = uv0 / tile_size - uv_tm;
	// coordinate inside a tile (0, 1)
    vec2 uvi = floor(uv_tmfr * tile_size);
	// coordinate inside a tile (0, tile_size)
    if (uv_inside_aabb(uv0)) {
      vec4 ti_val = <%texture2d/> // lookup the tilemap
	(sampler_tilemap, uv_tm / tilemap_size);
      vec2 uv_pixel = floor(ti_val.xy * 255.0 + 0.5) * tile_size + uvi;
	      // tile pattern coordinate
      tex_val1 = <%texture2d/> // lookup the tilepattern
	(sampler_pmpat, uv_pixel / tiletex_size);
    } else {
      tex_val1 = vec4(0.0, 0.0, 0.0, 0.0);
    }
  }

  vec2 parallax_next(in vec3 tsub, out vec3 tsub_next, in float z, in vec3 d)
  {
    tsub.xy = clamp(tsub.xy, 0.0, 1.0);
    vec2 r = vec2(0.0, 0.0);
    float dypos = float(d.y > 0.0);
    float nx = tsub.x + (dypos - tsub.y) * d.x / d.y;
    if (d.y != 0.0 && nx > 0.0 && nx < 1.0) {
      float nz = tsub.z + (dypos - tsub.y) * d.z / d.y;
      tsub_next = vec3(nx, dypos, nz);
      r.y = dypos * 2.0 - 1.0;
    } else if (d.x != 0.0) {
      float dxpos = float(d.x > 0.0);
      float ny = tsub.y + (dxpos - tsub.x) * d.y / d.x;
      float nz = tsub.z + (dxpos - tsub.x) * d.z / d.x;
      tsub_next = vec3(dxpos, ny, nz); // clamp ny?
      r.x = dxpos * 2.0 - 1.0;
    } else {
      tsub_next = vec3(tsub.xy, z);
    }
    return r;
  }

  void parallax_move_zcrop(in vec3 dir, inout vec4 tval, inout vec2 tpos,
   inout vec3 tsub)
  {
    tsub.xy = clamp(tsub.xy, 0.001, 0.999);
    dir /= max(abs(dir.x), abs(dir.y)); // FIXME: compute by caller?
      // W = unused(8), Z = depth(8), Y = CNN(4) CNP(4), X = CPN(4) CPP(4)
    float cvt = floor((dir.x > 0.0 ? tval.x : tval.y) * 255.0 + 0.5);
    float cval = dir.y > 0.0
      ? fract(cvt / 16.0) * 16.0
      : floor(cvt / 16.0);
    cval = min(cval, max(0.0, (tval.z - tsub.z - 0.001) / dir.z)); // crop cval
    // if (cval > 3.0) { dbgval.r = 1.0; } // FIXME
    // cval = 0; // FIXME
    // if (dir.x <= 0.0 && dir.y > 0.0) { cval = 0; } // FIXME
    vec3 delta = clamp_to_border(tpos + tsub.xy, dir * cval);
    // vec3 delta = dir * cval; // FIXME
    vec2 npos = tpos + tsub.xy + delta.xy;
    tpos = floor(npos);
    tsub = vec3(npos - tpos, tsub.z + delta.z);
    parallax_read(tpos, tval);
  }

  void parallax_move_nozcrop(in vec3 dir, inout vec4 tval, inout vec2 tpos,
   inout vec3 tsub)
  {
    tsub.xy = clamp(tsub.xy, 0.001, 0.999);
    dir /= max(abs(dir.x), abs(dir.y)); // FIXME: compute by caller?
    float cvt = floor((dir.x > 0.0 ? tval.x : tval.y) * 255.0 + 0.5);
    float cval = dir.y > 0.0
      ? fract(cvt / 16.0) * 16.0
      : floor(cvt / 16.0);
    // if (cval > 3.0) { dbgval.g = 1.0; } // FIXME
    // cval = 0; // FIXME
    // cval = max(0.0, cval - 0.1); // FIXME??
    vec3 delta = clamp_to_border(tpos + tsub.xy, dir * cval);
    // vec3 delta = dir * cval; // FIXME
    vec2 npos = tpos + tsub.xy + delta.xy;
    tpos = floor(npos);
    tsub = vec3(npos - tpos, tsub.z + delta.z);
    parallax_read(tpos, tval);
  }

  void parallax_loop(inout vec2 pos, in vec3 eye, in vec3 light,
    out float z, out vec3 nor, out bool vertical, out float shval)
  {
    z = 0.0;
    nor = vary_normal;
    vertical = false;
    shval = 1.0;
    eye.z /= tile_size;
    light.z /= tile_size;
    vec2 pos_m = pos; // texel coord
    vec2 tpos = floor(pos_m);
    vec3 tsub = vec3(pos_m - tpos, 0.0);
    vec4 tval;
    parallax_read(tpos, tval);
    if (tval.z == 0.0) {
      return;
    }
    vec2 tnext_diff = vec2(0.0, 0.0);
    for (int i = 0; i < 128; ++i) {
      vec3 tsub_next;
      tnext_diff = parallax_next(tsub, tsub_next, tval.z, eye);
      if (tsub_next.z >= tval.z) { // horizontal surface
	if (tsub_next.z > tsub.z) {
	  float ra = (tval.z - tsub.z) / (tsub_next.z - tsub.z);
	  tsub += (tsub_next - tsub) * ra;
	}
	tsub.z = tval.z;
	break;
      }
      tsub = tsub_next;
      tpos += tnext_diff;
      tsub.xy -= tnext_diff;
      parallax_move_zcrop(eye, tval, tpos, tsub);
      if (tval.z < tsub.z) { // vertical surface
	vertical = true;
	nor = vary_tangent * (-tnext_diff.x)
	  + vary_binormal * (-tnext_diff.y);
	break;
      }
      // if (i > 32) { dbgval.g = 1.0; }
    } // for (i)
    pos = (tpos + clamp(tsub.xy, 0.001, 0.999));
    z = tval.z;
    <%if><%enable_parallax_shadow/>
      if (vertical) {
	// move to the previous texel, which is not be filled
	tpos -= tnext_diff;
	tsub.xy += tnext_diff;
	tsub.xy = clamp(tsub.xy, 0.001, 0.999);
	parallax_read(tpos + tsub.xy, tval); // FIXME??
      }
      if (light.z < 0.0) {
	for (int i = 0; i < 128; ++i) {
	  vec3 tsub_next;
	  vec2 tnext_diff = parallax_next(tsub, tsub_next, 0.0, light);
	  if (tsub_next.z <= 0.0) {
	    break;
	  }
	  tsub = tsub_next;
	  tpos += tnext_diff;
	  tsub.xy -= tnext_diff;
	  parallax_move_nozcrop(light, tval, tpos, tsub);
	  if (tval.z < tsub.z) {
	    shval = 0.0;
	    break;
	  }
	  // if (i > 32) { dbgval.b = 1.0; }
	} // for (i)
      } // if (light.z < 0.0)
    <%/>
  }

<%/> // enable_parallax

vec3 reflection_fresnel(in float cos_l_n, in vec3 fresnel0)
{
  float fre_exp = pow(1.0 - cos_l_n, 5.0);
  return fresnel0 + (vec3(1.0, 1.0, 1.0) - fresnel0) * fre_exp;
}

vec3 light_specular_brdf(in float cos_l_h, in float cos_n_h,
  in float cos_n_v, in float cos_n_l, in float cos_v_h, float alpha,
  in vec3 material_specular)
{
  float alpha2 = alpha * alpha;
  float den = cos_n_h * cos_n_h * (alpha2 - 1.0) + 1.0;
  float distribution = alpha2 / (3.141592 * den * den);
  float geo = 1.0;
  vec3 fresnel = reflection_fresnel(cos_l_h, material_specular);
  return fresnel * (distribution * geo * 3.141592 / 4.0);
}

vec3 light_diffuse(in float cos_n_l, in vec3 material_diffuse)
{
  return material_diffuse * cos_n_l / 3.141592;
}

vec3 light_all(in vec3 light_color, in float lstr, in vec3 mate_specular,
  in vec3 mate_diffuse, in vec3 mate_emit, in float mate_alpha,
  in vec3 camera_dir, in vec3 light_dir, in samplerCube samp, in vec3 nor,
  in bool vertical, in float ambient, in float local_light_str,
  in vec3 local_light)
{
  vec3 half_l_v = normalize(light_dir + camera_dir);
  float cos_l_h = clamp(dot(light_dir, half_l_v), 0.0, 1.0);
  float cos_n_h = clamp(dot(nor, half_l_v), 0.0, 1.0);
  float cos_n_l = clamp(dot(nor, light_dir), 0.0, 1.0);
  float cos_n_v = clamp(dot(nor, camera_dir), 0.0, 1.0);
  float cos_v_h = cos_l_h;
  vec3 specular = 
    light_specular_brdf(cos_l_h, cos_n_h, cos_n_v, cos_n_l, cos_v_h,
      mate_alpha, mate_specular) * lstr;
  vec3 diffuse = light_diffuse(cos_n_l * lstr + cos_n_v * ambient,
    mate_diffuse);
  vec3 light_sp_di = light_color * (specular + diffuse) + mate_emit;
  float cos_n_ll = max(dot(nor, local_light), 0.0);
  // local_light_str = min(local_light_str, 1.0 - lstr);
  light_sp_di += vec3(1.0, 0.9, 0.6) *
    mate_diffuse * local_light_str * cos_n_ll;
  vec3 reflection_vec = reflect(-camera_dir, nor);
  vec3 sampler_vec = mate_diffuse.x > 0.0
    ? vec3(0.01, 0.01, 0.2) : reflection_vec;
  float env_coefficient = 0.125;
  vec3 env = <%texture_cube/>(sampler_env, sampler_vec).xyz * env_coefficient;
  env = env * env * reflection_fresnel(
    clamp(dot(nor, reflection_vec), 0.0, 1.0), mate_specular)
    * (vertical ? mate_specular : vec3(1.0, 1.0, 1.0))
    * clamp(1.0 - mate_alpha * 16.0, 0.0, 1.0);
  return light_sp_di + env;
  // return clamp(light_sp_di + env, 0.0, 1.0);
}

vec4 get_sampler_sm(int i, vec2 p)
{
  <%if><%is_gl3/>
    return <%texture2d/>(sampler_sm[i], p);
  <%else/>
    <%for x 0><%smsz/>
      if (i == <%x/>) return <%texture2d/>(sampler_sm[<%x/>], p);
    <%/>
    return vec4(0.0);
  <%/>
}

void main(void)
{
  vec4 color = vec4(0.0, 0.0, 0.0, 1.0);
  vec3 nor = vary_normal;
  vec3 rel_camera_pos = camera_pos - vary_position;
  vec3 camera_dir = normalize(rel_camera_pos);
  float frag_distance = length(rel_camera_pos);
    // TODO: このへんの変数の計算方法はraycastの後でやる
  float distbr = clamp(16.0 / frag_distance, 0.0, 1.0);
  float lstr = 1.0;
  float lstr_para = 1.0;
  float para_zval = 0.0;
  bool vertical = false;
  float ambient = 0.005;
  vec4 tex_val0 = vec4(0.5, 0.5, 0.5, 1.0);
  vec4 tex_val1 = vec4(0.5, 0.5, 0.5, 1.0);
  float frag_randval = generate_random(vec3(gl_FragCoord.xy, 0.0));
    // フラグメントの座標から生成した乱数
  float dist_rnd = frag_randval * 0.125;
  int miplevel = 0;
  <%if><%eq><%stype/>1<%/>
    // raycast
    <%if><%is_gl3_or_gles3/>
      mat3 normal_matrix = mat3(vary_model_matrix);
    <%else/>
      mat4 mm = vary_model_matrix;
      mat3 normal_matrix = mat3(mm[0].xyz, mm[1].xyz, mm[2].xyz);
    <%/>
    nor = vary_normal * normal_matrix;
    vec3 camera_local = -camera_dir * normal_matrix;
      // カメラから物体への向き、接線空間
    vec3 light_local = light_dir * normal_matrix;
      // 光源への向き、接線空間
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
	nor = voxel_get_next(pos, aabb_min, aabb_max, -camera_local, pos_n);
	pos = pos_n;
      }
      pos = clamp(pos, aabb_min + epsi, aabb_max - epsi);
    <%/>
    <%if><%eq><%update_frag_depth/>1<%/>
    <%if><%eq><%stype/>1<%/>
      // このフラグメントのdepth値は、raycastするまでもなく、raycast始点で
      // あるposのdepth値以上になることはわかっている。もしすでにdepth_rdの
      // 値がそれより小さいなら、このフラグメントが描画されることはないので
      // 無駄にraycast計算しないようdiscardする。
      // stype==1のシェーダはssubtypeの大きいほうから順に描画し、depth_rdに
      // は毎回その時点のデプスバッファのコピーを保持している。
      <%if><%check_frag_depth/>
      <%if><%is_gl3_or_gles3/>
      float prev_depth = texelFetch(sampler_depth_rd, ivec2(gl_FragCoord.xy),
	0).x;
      <%else/>
      float prev_depth = 1.0f; // TODO?
      <%/>
      if (!cam_inside_aabb) {
	vec3 ini_tngpos = vary_aabb_or_tconv.xyz + pos * vary_aabb_or_tconv.w;
	vec4 ini_gpos = vary_model_matrix * vec4(ini_tngpos, 1.0);
	vec4 ini_vpos = view_projection_matrix * ini_gpos;
	/*
	float ini_depth = (ini_vpos.z / ini_vpos.w + 1.0) * 0.5;
	*/
	// 誤差によりini_vpos.wが0以下になることがある。
	float ini_depth = ini_vpos.w <= 0.0
	  ? 0.0 : (ini_vpos.z / ini_vpos.w + 1.0) * 0.5;
	/*
	if (ini_vpos.w <= -0.0) {
	  <%fragcolor/> = vec4(1.0,1.0,0.0,1.0); return; 
	}
	*/
	if (prev_depth < ini_depth) {
	  discard;
	}
      }
      <%/>
    <%/>
    <%/>
    /*
    float dist_pos_campos_2 = dot(pos - campos, pos - campos) + 0.0001;
    float dist_log2 = log(dist_pos_campos_2) * 0.5 / log(2.0);
    // if (dist_log2 < -3.0) { <%fragcolor/> = vec4(1,0,1,1); return; }
    int miplevel = clamp(int(dist_log2 + dist_rnd + 6.5), 0, 8);
    */
    int miplevel_noclamp = raycast_get_miplevel(pos, campos, dist_rnd);
    if (option_value2 >= 0.0) {
      miplevel_noclamp = int(option_value2);
    }
    miplevel = clamp(miplevel_noclamp, 0, 8);
    // if (miplevel > 2) { <%fragcolor/> = vec4(1,0,1,1); return; }
    // if (miplevel > 1) { <%fragcolor/> = vec4(1,1,0,1); return; }
    // if (miplevel > 0) { <%fragcolor/> = vec4(1,0,0,1); return; }
    // if (int(option_value + 0.5) == 1) { miplevel = 0; }
    int hit = -1;
    // float selfshadow_para = clamp(1.0 - dist_log2 * 0.1, 0.0, 1.0);
    float selfshadow_para = 0.0f;
    //if (option_value2 >= 0.0) {
      hit = raycast_tilemap(pos, campos, dist_rnd, camera_local, light_local,
	aabb_min, aabb_max, tex_val0, tex_val1, nor, selfshadow_para, lstr_para,
	miplevel, option_value2 < 0.0);
    //} else {
    //  hit = raycast_tilemap(pos, campos, dist_rnd, camera_local, light_local,
    //	aabb_min, aabb_max, tex_val1, nor, selfshadow_para, lstr_para,
    //	miplevel);
    //}
    // if (hit == 1)  { <%fragcolor/> = vec4(1.0, 0.0, 0.0, 1.0); return; }
    /*
    hit = raycast_waffle(pos, fragpos, camera_local,
      aabb_min, aabb_max, 0);
    if (hit < 0) { <%fragcolor/> = vec4(1.0, 0.5, 0.5, 1.0); return; }
    if (hit >= 0) { <%fragcolor/> = vec4(0.5, 0.5, 0.5, 1.0); return; }
    */
    <%if><%eq><%get_config dbgval/>1<%/>
    if (dbgval.a > 0.0) { <%fragcolor/> = dbgval; return; }
    <%/>
    if (hit < 0) {
      // <%fragcolor/> = vec4(1.0); return;
      discard;
    }
    if (cam_inside_aabb && hit <= (option_value2 < 0 ? 1 : 0)) {
      /* カメラが物体にめりこんでいる。depth計算で0除算がおきないよう
       * ここでreturnする。mip_detailの処理によりhit == 1でもめりこんでいる
       * ことがありうる(option_value2が-1のとき)。 */
      <%fragcolor/> = vec4(0.0);
      <%if><%eq><%update_frag_depth/>1<%/>
	gl_FragDepth = -1.0;
      <%/>
      return;
    }
    // ambient = max(0.0, 0.005 - float(hit) * 0.0001);
    nor = normal_matrix * nor; // local to global
    vec3 frag_tanpos = vary_aabb_or_tconv.xyz + pos * vary_aabb_or_tconv.w;
      // posをテクスチャ座標から接線空間の座標に変換
    vec4 frag_gpos = vary_model_matrix * vec4(frag_tanpos, 1.0);
      // vary_model_matrixは接線空間からワールドへの変換
    frag_distance = length(frag_gpos.xyz - camera_pos);
      // frag_distanceを更新。合っているか？
    <%if><%eq><%stype/>1<%/>
      vec4 frag_vpos = view_projection_matrix * frag_gpos;
      float frag_depth = (frag_vpos.z / frag_vpos.w + 1.0) * 0.5;
      <%if><%eq><%update_frag_depth/>1<%/>
	<%if><%check_frag_depth/>
	if (prev_depth < frag_depth) {
          // より近くに描画されたフラグメントが既に存在するので描かない
	  discard;
	}
	<%/>
	// FragDepthを更新する
	gl_FragDepth = frag_depth;
      <%/>
    <%/>
  <%elseif/><%enable_normalmapping/>
    // stype==0, enable_normalmapping
    vec2 uv0 = vary_uvw.xy;
    <%if><%enable_parallax/>
      vec4 para_val;
      vec3 eye_tan = vec3( // eye vector, tangent space
	-dot(camera_dir, vary_tangent),
	-dot(camera_dir, vary_binormal),
	dot(camera_dir, vary_normal));
      vec3 light_tan = vec3( // light vector, tangent space
	dot(light_dir, vary_tangent),
	dot(light_dir, vary_binormal),
	-dot(light_dir, vary_normal));
      parallax_loop(uv0, eye_tan, light_tan, para_zval, nor, vertical,
	lstr_para);
    <%/>
    vec2 subtex_uv;
    tilemap(uv0, tex_val1, subtex_uv);
    float alv0 = floor(tex_val1.a * 255.0 + 0.5);
    float avol = floor(alv0 / 16.0);
    int alv = int(alv0 - avol * 16.0 + 0.5);
    if (!vertical) {
      float lt = float(subtex_uv.y - subtex_uv.x >= 0.0);
      float lb = float(subtex_uv.y + subtex_uv.x <= 1.0);
      vec4 avlv = vec4(float(alv == 1), float(alv == 3),
	float(alv == 5), float(alv == 7));
      float ut1 = dot(avlv, vec4(-lt, 1.0-lb, 1.0-lt, -lb));
      float vt1 = dot(avlv, vec4(lt-1.0, -lb, lt, 1.0-lb));
      ut1 += float(alv == 4);
      vt1 -= float(alv == 2);
      float distbr2 = clamp(16.0 / frag_distance , 0.001, 1.0);
      avol = (avol - 8.0) * distbr2 * 0.2;
      vec3 nor_delta = vary_tangent * ut1 * avol
	+ vary_binormal * vt1 * avol;
      nor = normalize(nor + nor_delta);
    }
  <%else/>
  //
  <%/> // endif stype == 1
  <%if><%enable_shadowmapping/>
    int sm_to_use = 0;
    vec3 cp_sm = vec3(0.0); // shadowmapの視錐台でのカメラ位置
    <%if><%eq><%stype/>1<%/>
      // vec3 ndelta = mat3(shadowmap_vp[0]) * vary_normal * ndelta_scale;
	// 0.02
      bool smu_done = false;
      <%for x 0><%smsz/>
	if (!smu_done) {
	  cp_sm = (shadowmap_vp[<%x/>] * frag_gpos).xyz;
	  if (max_vec3(abs(cp_sm)) < 0.8) {
	    sm_to_use = <%x/>;
	    smu_done = true;
	  }
	}
      <%/>
      /*
      */
      /*
      cp_sm = (shadowmap_vp[0] * frag_gpos).xyz;
      if (max_vec3(abs(cp_sm)) > 0.8) {
	sm_to_use = 1;
	cp_sm = (shadowmap_vp[1] * frag_gpos).xyz;
	if (max_vec3(abs(cp_sm)) > 0.8) {
	  sm_to_use = 2;
	  cp_sm = (shadowmap_vp[2] * frag_gpos).xyz;
	  if (max_vec3(abs(cp_sm)) > 0.8) {
	    sm_to_use = 3;
	    cp_sm = (shadowmap_vp[3] * frag_gpos).xyz;
	  }
	}
      }
      */
      /*
      for (sm_to_use = 0; sm_to_use < <%smsz/>; ++sm_to_use) {
	vec4 sp = shadowmap_vp[sm_to_use] * frag_gpos;
	cp_sm = sp.xyz; // / sp.w; //  + ndelta / d;
	if (max_vec3(abs(cp_sm)) < 0.8 || sm_to_use + 1 == <%smsz/>) {
	  break;
	}
      }
      */
      // FIXME: remove
      /*
      if (sm_to_use == 1) {
        // <%fragcolor/> = vec4(1.0, 0.0, 0.0, 1.0); return;
      }
      if (sm_to_use == 3) {
        // <%fragcolor/> = vec4(0.0, 1.0, 0.0, 1.0); return;
      }
      if (max_vec3(abs(cp_sm)) > 1.0) {
        // <%fragcolor/> = vec4(0.0, 1.0, 0.0, 1.0); return;
      }
      if (frag_distance > shadowmap_max_distance) {
        <%fragcolor/> = vec4(0.0, 1.0, 0.0, 1.0); return;
      }
      */
    <%else/>
      // stype != 1。このときはvsでvary_smposaを計算清み。
      for (sm_to_use = 0; sm_to_use < <%smsz/>; ++sm_to_use) {
	cp_sm = vary_smposa[sm_to_use];
	if (max_vec3(abs(cp_sm)) < 0.8 || sm_to_use + 1 == <%smsz/>) {
	  break;
	}
      }
    <%/>
    <%if><%enable_depth_texture/>
      <%if><%enable_shadowmapping_multisample/>
	vec3 smpos;
	smpos = (cp_sm + 1.0) * 0.5;
	// float zval = 0.0;
	float zval_cur = 0.0;
	float sml = 0.0;
	float idxmin = -1.0;
	float idxmax = 1.0;
	for (float i = idxmin; i <= idxmax; ++i) {
	  for (float j = idxmin; j <= idxmax; ++j) {
	    vec2 c = smpos.xy + 
		(vec2(i,j) + generate_random(rel_camera_pos) * 1.0)/4096.0;
	    /*
	    if (max(c.x, c.y) > 1.0 || min(c.x, c.y) < 0.0)
	      { <%fragcolor/> = vec4(1.0); return; }
	    */
	    zval_cur = get_sampler_sm(sm_to_use, c).x;
	    // zval = min(zval, zval_cur);
	    float sm_min_dist = 0.0001; // FIXME: 調整必要
	    /*
	    sml += float(smpos.z < sm_min_dist + zval_cur
	     * (1.005 + (abs(i)+abs(j))/4096.0))/9.0;
	    */
	    sml += float(smpos.z < sm_min_dist + zval_cur * 1.005)/9.0;
	    /* abs(i)+abs(j)/4096.0 を加えるとmacosxでおかしい？ */
	  }
	}
// FIXME
//if (sml < 0.8) { <%fragcolor/> = vec4(0.0, 1.0, 1.0, 1.0); return; }
      <%else/>
	// no multisample
	vec3 smpos;
	smpos = (cp_sm + 1.0) * 0.5;
	float zval = get_sampler_sm
	  (sm_to_use, smpos.xy).x;
	float sml = float(smpos.z < zval * 1.0005);
      <%/>
    <%elseif/><%enable_vsm/>
      // vsm, no depth texture
      vec3 smpos;
      vec2 smz;
      float dist;
      float variance;
      <%if><%enable_shadowmapping_multisample/>
	smpos = (cp_sm + 1.0) * 0.5;
	float zval = 99999.0;
	float sml = 0.0;
	// 4箇所サンプリングする
	for (int i = -1; i <= 1; i += 2) {
	  for (int j = -1; j <= 1; j += 2) {
	    // キャスト位置
	    smz = get_sampler_sm
	      (sm_to_use, smpos.xy
		+ vec2(float(i), float(j)) / 8192.0).rg;
	    zval = min(zval, smz.r);
	    dist = smpos.z - smz.r * 1.001;
	      // フラグメント位置からキャスト位置を引く
	    if (dist <= 0.0) {
	      // キャスト位置のほうが遠いので影でない
	      sml += 1.0 / 4.0;
	    } else {
	      // キャスト位置のほうが近い
	      variance = smz.g - smz.r * smz.r;
	      variance = max(variance, 0.000001);
	      sml += (1.0 / 4.0) * variance / (variance + dist * dist);
	    }
	  } // for (j)
	} // for (i)
	// 少量の光漏れを影にする
	float bias = clamp(0.2 + dist * (float(sm_to_use) * 0.0 + 0.0),
	  0.0, 1.0);
	sml = clamp(sml * (1.0 + bias) - bias, 0.0, 1.0);
      <%else/>
	// no multisample
	smpos = (cp_sm + 1.0) * 0.5;
	smz = get_sampler_sm
	  (sm_to_use, smpos.xy).rg;
	float zval = smz.r;
	float zval_sq = smz.g;
	float sml = 0.0;
	dist = smpos.z - smz.r * 1.001;
	if (dist <= 0.0) {
	  sml = 1.0;
	} else {
	  variance = smz.g - smz.r * smz.r;
	  variance = max(variance, 0.000001);
	  sml = clamp(
	   variance / (variance + dist * dist) * 1.5 - 0.1, 0.0, 1.0);
	}
      <%/>
    <%else/>
      // no depth texture, no vsm
      vec3 smpos;
      vec3 smz;
      smpos = (cp_sm + 1.0) * 0.5;
      smz = get_sampler_sm
	  (sm_to_use, smpos.xy).rgb;
      smz = floor(smz * 255.0 + 0.5);
      float zval = 
	smz.r / 256. + smz.g / 65536.0 + smz.b / 16777216.;
      float sml = float(smpos.z < zval * 1.0005);
    <%/>
    // 距離に応じて影の濃さを調整
    float sdistp = clamp(3.0 - frag_distance * 4.0 / shadowmap_max_distance,
      0.0, 1.0);
    // sml = sml * sdistp * 0.5 + (1.0 - sdistp) * 0.5;
    sml = sml * sdistp + (1.0 - sdistp) * 0.67;
    // sml = 1.0;
    float smv0 = min(1.0, sml);
    /*
    if (sm_to_use == 1) { <%fragcolor/> = vec4(1.0, smv0, 0.0, 1.0); return; }
    */
    lstr = min(lstr, smv0);
    <%if><%eq><%stype/>1<%/>
      lstr = clamp(dot(nor, light_dir) * 4.0, 0.0, lstr);
    <%else/>
      lstr = clamp(dot(vary_normal, light_dir) * 4.0, 0.0, lstr);
    <%/>
    // lstr = clamp(dot(vary_normal, light_dir) * 32.0, 0.0, lstr);
    // lstr = min(lstr, float(dot(vary_normal, light_dir) > 0.0));
  <%/> // endif enable_shadowmapping
  // fresnel
  // float cos_v_h = clamp(dot(camera_dir, half_l_v), 0.0, 1.0);
  float mate_alpha = 0.2; // tex_val1.a + 0.001;
  vec3 mate_specular = vec3(0.04, 0.04, 0.04);
  vec3 mate_diffuse = vec3(0.0, 0.0, 0.0);
  vec3 mate_emit = vec3(0.0);
  <%if><%eq><%stype/>1<%/>
  if (false) {
    // FIXME テクスチャテスト中
    if (miplevel < 1.3f + dist_rnd * 0.25f) {
      vec3 spos = pos * 512.0;
      float v = clamp(fnoise3(floor(spos) / 32768.0) * 2.0, 0.0, 1.0);
      if (v > 0.5) {
        float spxf = fract(spos.x * 64.0);
        if (spxf < 0.5) {
          tex_val1.rgb = vec3(1.0, 0.8, 0.0);
        } else {
          tex_val1.rgb = vec3(0.0);
        }
      }
    }
  }
  {
    // tex_val1.a = 0.0;
    float aval = floor(tex_val1.a * 255.0 + 0.5);
    float aval_me = floor(aval / 64.0);
    aval = aval - aval_me * 64.0;
    float aval_roughness = aval;
    lstr = max(0.0, lstr - float(miplevel) * 0.125 * 0.5);
      // miplevelが上がると暗く
    if (aval_me == 1.0 || aval_me == 3.0) {
      // emission
      mate_emit = tex_val0.rgb; // FIXME これ正しいのか？
    }
    if (aval_me == 2.0 || aval_me == 3.0) {
      // metal
      mate_specular = tex_val1.rgb;
    } else {
      // 0か3なら非金属
      mate_diffuse = tex_val1.rgb;
    }
    /*
    float aval_roughness = floor(aval / 16.0);
    aval = aval - aval_roughness * 16.0;
    float aval_metalness = floor(aval / 2.0);
    aval = aval - aval_metalness * 2.0;
    float aval_emission = aval;
    if (aval_emission == 0.0) {
      if (aval_metalness == 0.0) {
	mate_diffuse = tex_val1.rgb;
      } else {
	mate_specular = tex_val1.rgb;
      }
    } else {
      mate_emit = tex_val1.rgb * (aval_emission + 1.0) / 8.0;
    }
    */
    float p = (aval_roughness + 1.0) / 16.0;
    mate_alpha = p * p;
  }
  <%else/>
  {
    if (max(tex_val1.r, max(tex_val1.g, tex_val1.b)) > 0.9) {
      mate_specular = tex_val1.rgb;
    } else {
      mate_diffuse = tex_val1.rgb;
    }
  }
  <%/>
  if (int(option_value + 0.5) == 1) {
    mate_alpha = 0.03;
    mate_specular = vec3(0.95, 0.64, 0.54);
    mate_diffuse = vec3(0.0, 0.0, 0.0);
  }
  if (int(option_value + 0.5) == 2) {
    mate_alpha = 0.03;
    mate_specular = vec3(0.95, 0.93, 0.88);
    mate_diffuse = vec3(0.0, 0.0, 0.0);
  }
  if (int(option_value + 0.5) == 3) {
    mate_alpha = 0.1;
    mate_specular = vec3(0.04, 0.04, 0.04);
    mate_diffuse = vec3(0.6, 0.7, 0.4);
  }
  float local_light_str = 0.0;
  vec3 local_light = vec3(0.0);
  <%if><%eq><%stype/>1<%/>
    /*
    if (int(tex_val1.a * 255.0 + 0.5) == 1) {
      // mate_emit = tex_val1.rgb / 4.0;
      // mate_alpha = 0.0;
      // mate_specular = vec3(0.0);
      // mate_diffuse = vec3(0.0);
      local_light = - 0.5 + fract(pos * map3_size);
	// ローカルライトの相対位置
      local_light_str = clamp(0.125 / dot(local_light, local_light), 0.0, 1.0);
      local_light = normalize(local_light);
    }
    */
    // FIXME?
    if (frag_depth < 1.7f + dist_rnd * 0.25f)
    {

      // mate_specular.r += clamp(fnoise3(pos), 0.1, 1.0);
      // mate_diffuse.r += clamp(fnoise3(pos), 0.1, 1.0);

      // float v = clamp(fnoise3(pos / 16.0) * 2.0, 0.0, 1.0);
      // if (v < 0.001) {
        // mate_emit = vec3(0.8, 1.0, 1.0);
      // }

      // float v1 = clamp(fnoise3(pos / 1024.0) * 4.0, 0.0, 1.0);
      // v1 = pow(v1, 16.0);
      // v = pow(min(v, v1), 16.0);
      // mate_emit = vec3(v * 0.9, v * 2.0, v * 2.0) * clamp(16.0 - frag_distance * 0.9, 0.0, 0.5);


      // mate_alpha = clamp(pnoise3(pos * 82492.0) * 1.01, 0.01, 1.0);
      // mate_alpha = 0.01;
      // <%fragcolor/> = vec4(1.0,1.0,0.0,1.0); return;
      /*
      mate_alpha = clamp(mate_alpha - snoise(pos * 1024.0 * 256.0) / 1.0f,
	0.0, 0.5);
      */
      /*
      vec2 nt_coord = fract(vec2(pos.x + pos.z, pos.y + pos.z) * 256.0);
      float nval = texelFetch(sampler_noise,
	// ivec2(gl_FragCoord.xy),
	ivec2(nt_coord * 1024.0),
	0).r;
      */
      // mate_diffuse = vec3(nval, nval, nval);
      // mate_specular = vec3(nval, nval, nval);
      // mate_alpha = 0.3;
      // mate_alpha = clamp(mate_alpha - nval / 8.0f, 0.0, 0.5);
      // float p = 7.0 / 32.0;
      // mate_alpha = p * p;
    }
    /*
    */
    if (frag_distance < 0.0001) {
      // <%fragcolor/> = vec4(1.0); return;
    }
    // ambient = clamp(float(10 - hit) / 1024.0, 0.0, 0.0125);
    // ambient = clamp(1.0 / (frag_distance * 1024.0), 0.0, 0.0125);
    ambient = clamp(1.0 / (frag_distance * 128.0), 0.0, 0.025);
  <%/>
  vec3 li1 = light_all(vec3(1.0, 1.0, 1.0), lstr * lstr_para, mate_specular,
    mate_diffuse, mate_emit, mate_alpha, camera_dir,
    light_dir, sampler_env, nor, vertical, ambient, local_light_str,
    local_light);
  /*
  // vec3 light_color2 = vec3(1.0, 1.0, 1.0) * lstr;
  // vec3 light_color1 = light_color2 * lstr_para;
  vec3 li2 = light_all(light_color2, mate_specular, mate_diffuse,
    mate_alpha, camera_dir, light_dir, sampler_env,
    normalize(vary_normal), false, ambient);
  color.xyz += sqrt(mix(li2, li1, distbr));
  */
  li1 *= exposure;
  li1 = min(li1, 1.0 - 0.5 / exp(li1));
  /// li1 = 1.0 - 1.0 / exp(li1);
  li1 = sqrt(li1);
  // vec3 v01 = clamp(li1, 0.0, 1.0);
  // vec3 ve = 1.0 - 1.0 / exp(li1 * 10.0);
  // color.xyz += mix(v01, ve, v01);
  /// color.xyz += 1.0 - 1.0 / exp(li1);
  color.xyz += li1;
  color.xyz += frag_randval * (1.0f / 256.0f); // reduce color banding
  <%fragcolor/> = color;
}

