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
uniform float exposure;
uniform float random_seed;
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
<%if><%eq><%get_config dbgval/>1<%/>
vec4 dbgval = vec4(0.0);
<%/>

<%if><%enable_shadowmapping/>
  <%if><%ne><%stype/>1<%/>
    <%frag_in/> vec3 vary_smposa[<%smsz/>];
  <%/>
<%/>

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

  void tilemap(in vec2 uv0, out vec4 tex_val, out vec2 subtex_uv)
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
      tex_val = <%texture2d/>(sampler_dpat, uv_pixel / tiletex_size);
	    // lookup the tilepattern
    } else {
      tex_val = vec4(0.5, 0.5, 0.5, 0.0);
    }
  }

<%/> // enable_normalmapping

<%if><%enable_parallax/>

  void parallax_read(in vec2 uv0, out vec4 tex_val)
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
      tex_val = <%texture2d/> // lookup the tilepattern
	(sampler_pmpat, uv_pixel / tiletex_size);
    } else {
      tex_val = vec4(0.0, 0.0, 0.0, 0.0);
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
  in vec3 mate_diffuse, in float mate_alpha, in vec3 camera_dir,
  in vec3 light_dir, samplerCube samp, in vec3 nor, bool vertical,
  in float ambient)
{
  vec3 half_l_v = normalize(light_dir + camera_dir);
  float cos_l_h = clamp(dot(light_dir, half_l_v), 0.0, 1.0);
  float cos_n_h = clamp(dot(nor, half_l_v), 0.0, 1.0);
  float cos_n_l = clamp(dot(nor, light_dir), 0.0, 1.0);
  float cos_n_v = clamp(dot(nor, camera_dir), 0.0, 1.0);
  float cos_v_h = cos_l_h;
  vec3 light_sp_di = light_color * (
    light_specular_brdf(cos_l_h, cos_n_h, cos_n_v, cos_n_l, cos_v_h,
      mate_alpha, mate_specular) * lstr +
    light_diffuse(cos_n_l * (lstr + ambient), mate_diffuse));
  vec3 reflection_vec = reflect(-camera_dir, nor);
  vec3 sampler_vec = mate_diffuse.x > 0.0
    ? vec3(0.01, 0.01, 0.2) : reflection_vec;
  float env_coefficient = 0.125;
  vec3 env = <%texture_cube/>(sampler_env, sampler_vec).xyz * env_coefficient;
  env = env * env * reflection_fresnel(
    clamp(dot(nor, reflection_vec), 0.0, 1.0), mate_specular)
    * (vertical ? mate_specular : vec3(1.0, 1.0, 1.0))
    * clamp(1.0 - mate_alpha * 16.0, 0.0, 1.0);
  return clamp(light_sp_di + env, 0.0, 1.0);
}

float generate_random(vec3 v)
{
  v.x += fract(random_seed);
  v.y += fract(random_seed);
  return fract(sin(dot(v.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

vec4 get_sampler_sm(int i, vec2 p)
{
  <%if><%is_gl3_or_gles3/>
    return <%texture2d/>(sampler_sm[i], p);
  <%else/>
    <%for x 0><%smsz/>
      if (i == <%x/>) return <%texture2d/>(sampler_sm[<%x/>], p);
    <%/>
    return vec4(0.0);
  <%/>
}

<%if><%eq><%stype/>1<%/>
float calc_depth_from_tngpos(vec3 tngpos)
{
  // tngposは接線空間の座標
  vec4 gpos = vary_model_matrix * vec4(tngpos, 1.0);
  vec4 vpos = view_projection_matrix * gpos;
  return (vpos.z / vpos.w + 1.0) * 0.5;
}
<%/>

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
  vec4 tex_val = vec4(0.5, 0.5, 0.5, 1.0);
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
      // 計算されるdepth値は、raycast始点であるposのdepth値以上になる。
      // もしすでにdepth_rdの値がそれより小さいならdiscardする。
      float prev_depth = texelFetch(sampler_depth_rd, ivec2(gl_FragCoord.xy),
	0).x;
      if (!cam_inside_aabb) {
	vec3 ini_tngpos = vary_aabb_or_tconv.xyz + pos * vary_aabb_or_tconv.w;
	vec4 ini_gpos = vary_model_matrix * vec4(ini_tngpos, 1.0);
	vec4 ini_vpos = view_projection_matrix * ini_gpos;
	float ini_depth = (ini_vpos.z / ini_vpos.w + 1.0) * 0.5;
	/*
	float ini_depth = ini_vpos.w <= 0.0 ? 0.0
	  : (ini_vpos.z / ini_vpos.w + 1.0) * 0.5;
	*/
	if (prev_depth < ini_depth) {
	  discard;
	}
      }
    <%/>
    <%/>
    float dist_pos_campos_2 = dot(pos - campos, pos - campos) + 0.01;
    float dist_rnd = generate_random(pos) * 0.25;
    float dist_log2 = log(dist_pos_campos_2) * 0.5 / log(2.0);
    int miplevel = clamp(int(dist_log2 + dist_rnd + 2.0), 0, 8);
    // if (miplevel > 2) { <%fragcolor/> = vec4(1,0,1,1); return; }
    // if (miplevel > 1) { <%fragcolor/> = vec4(1,1,0,1); return; }
    // if (miplevel > 0) { <%fragcolor/> = vec4(1,0,0,1); return; }
    // if (int(option_value + 0.5) == 1) { miplevel = 0; }
    int hit = -1;
    // float selfshadow_para = clamp(1.0 - dist_log2 * 0.1, 0.0, 1.0);
    float selfshadow_para = 0.0f;
    <%if><%eq><%get_config edit_mode/>1<%/>
    miplevel = 0;
    <%/>
    hit = raycast_tilemap(pos, camera_local, light_local,
      aabb_min, aabb_max, tex_val, nor, selfshadow_para, lstr_para, miplevel);

<%if><%eq><%ssubtype/>2<%/>
// if (hit >= 0)  { <%fragcolor/> = vec4(1.0, 0.0, 0.0, 1.0); return; }
<%/>
<%if><%eq><%ssubtype/>4<%/>
// if (hit >= 0) { <%fragcolor/> = vec4(1.0, 1.0, 0.0, 1.0); return; }
<%/>
<%if><%eq><%ssubtype/>5<%/>
// if (hit >= 0) { <%fragcolor/> = vec4(1.0, 0.0, 1.0, 1.0); return; }
<%/>
    /*
    hit = raycast_waffle(pos, fragpos, camera_local, light_local,
      aabb_min, aabb_max);
    if (hit > 0) {
      <%fragcolor/> = vec4(0.5, 0.5, 0.5, 1.0); return;
    }
    */
    <%if><%eq><%get_config dbgval/>1<%/>
    if (dbgval.a > 0.0) { <%fragcolor/> = dbgval; return; }
    <%/>
    if (hit < 0) {
      discard;
    }
    // ambient = max(0.0, 0.005 - float(hit) * 0.0001);
    nor = normal_matrix * nor; // local to global
    pos = vary_aabb_or_tconv.xyz + pos * vary_aabb_or_tconv.w;
      // posをテクスチャ座標から接線空間の座標に変換
    vec4 gpos = vary_model_matrix * vec4(pos, 1.0);
      // vary_model_matrixは接線空間からワールドへの変換
    <%if><%eq><%update_frag_depth/>1<%/>
    <%if><%eq><%stype/>1<%/>
      // FragDepth更新するならこの節を有効にする
      {
	vec4 vpos = view_projection_matrix * gpos;
	float depth = (vpos.z / vpos.w + 1.0) * 0.5;
	if (prev_depth < depth) {
	  discard;
	}
	gl_FragDepth = depth;
      }
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
    tilemap(uv0, tex_val, subtex_uv);
    float alv0 = floor(tex_val.a * 255.0 + 0.5);
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
    vec3 cp_sm; // shadowmapの視錐台でのカメラ位置
    <%if><%eq><%stype/>1<%/>
      // vec3 ndelta = mat3(shadowmap_vp[0]) * vary_normal * ndelta_scale;
	// 0.02
      for (sm_to_use = 0; sm_to_use < <%smsz/>; ++sm_to_use) {
	vec4 sp = shadowmap_vp[sm_to_use] * gpos;
	cp_sm = sp.xyz; // / sp.w; //  + ndelta / d;
	if (max_vec3(abs(cp_sm)) < 0.8 || sm_to_use + 1 == <%smsz/>) {
	  break;
	}
      }
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
//if (max(c.x, c.y) > 1.0 || min(c.x, c.y) < 0.0) { <%fragcolor/> = vec4(1.0); return; }
	    zval_cur = get_sampler_sm(sm_to_use, c).x;
	    // zval = min(zval, zval_cur);
	    sml += float(smpos.z < zval_cur
	     * (1.0005 + (abs(i)+abs(j))/4096.0))/9.0;
	  }
	}
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
    float smv0 = min(1.0, sml);
//if (sm_to_use == 1) { <%fragcolor/> = vec4(1.0, smv0, 0.0, 1.0); return; }
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
  float mate_alpha = 1.0;
  vec3 mate_specular = vec3(0.04, 0.04, 0.04);
  vec3 mate_diffuse = vec3(0.0, 0.0, 0.0);
  if (max(tex_val.r, max(tex_val.g, tex_val.b)) > 0.9) {
    mate_specular = tex_val.rgb;
  } else {
    mate_diffuse = tex_val.rgb;
  }
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
  vec3 li1 = light_all(vec3(1.0, 1.0, 1.0), lstr * lstr_para, mate_specular,
    mate_diffuse, mate_alpha, camera_dir, light_dir, sampler_env, nor,
    vertical, ambient);
  /*
  // vec3 light_color2 = vec3(1.0, 1.0, 1.0) * lstr;
  // vec3 light_color1 = light_color2 * lstr_para;
  vec3 li2 = light_all(light_color2, mate_specular, mate_diffuse,
    mate_alpha, camera_dir, light_dir, sampler_env,
    normalize(vary_normal), false, ambient);
  color.xyz += sqrt(mix(li2, li1, distbr));
  */
  li1 *= exposure;
  // li1 = min(li1, 1.0 - 0.5 / exp(li1));
  li1 = 1.0 - 1.0 / exp(li1);
  li1 = sqrt(li1);
  // vec3 v01 = clamp(li1, 0.0, 1.0);
  // vec3 ve = 1.0 - 1.0 / exp(li1 * 10.0);
  // color.xyz += mix(v01, ve, v01);
  /// color.xyz += 1.0 - 1.0 / exp(li1);
  color.xyz += li1;
  <%fragcolor/> = color;
}