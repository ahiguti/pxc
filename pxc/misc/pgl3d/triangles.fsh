
<%prepend/>
const float epsilon = 1e-6;
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
uniform vec3 camera_pos;
uniform vec3 light_dir;
uniform float light_on;
uniform float option_value;
<%frag_in/> vec3 vary_position;
<%frag_in/> vec3 vary_normal;
<%frag_in/> vec3 vary_tangent;
<%frag_in/> vec3 vary_binormal;
<%frag_in/> vec3 vary_uvw;
<%frag_in/> vec4 vary_uv_aabb;
<%if><%not><%eq><%opt/>0<%/><%/>
  // uniform sampler1D sampler_voxpat;
  uniform sampler3D sampler_voxpat;
  <%frag_in/> mat4 vary_model_matrix;
  <%frag_in/> vec3 vary_position_local;
  <%frag_in/> vec3 vary_camerapos_local;
<%/>
<%decl_fragcolor/>

<%if><%enable_shadowmapping/>

  <%if><%not><%light_fixed/><%/>
    <%frag_in/> vec3 vary_smposa[<%smsz/>];
  <%else/>
    uniform float ndelta_scale; // 0.02 / 40.0
  <%/>

  float linear_01(in float x, in float a, in float b)
  {
    return clamp((x - a) / (b - a), 0.0, 1.0);
  }

  float linear_10(in float x, in float a, in float b)
  {
    return clamp((b - x) / (b - a), 0.0, 1.0);
  }

  float max_vec3(in vec3 v)
  {
    return max(v.x, max(v.y, v.z));
  }

  float max3(in float x0, in float x1, in float x2)
  {
    return max(x0, max(x1, x2));
  }

<%/> // enable_shadowmapping

bool uv_inside_aabb(in vec2 uv)
{
  vec4 aabb = floor(vary_uv_aabb + 0.5);
  return uv.x >= aabb.x && uv.y >= aabb.y && uv.x < aabb.z && uv.y < aabb.w;
}

vec3 clamp_to_border(in vec2 uv, in vec3 delta, inout vec4 dbg)
{
  vec4 aabb = floor(vary_uv_aabb + 0.5);
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
    // if (np.x >= aabb.z || np.y >= aabb.w) { dbg.r = 1.0; }
    // if (ratx < 0 || raty < 0) { dbg.b = 1.0; }
    // if (!uv_inside_aabb(np)) { dbg.r = 1.0; }
    // if (np.x < aabb.x || np.y < aabb.y ||
    //    np.x >= aabb.z || np.y >= aabb.w) { dbg.r = 1.0; }
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

<%if><%not><%eq><%opt/>0<%/><%/>

  bool pos3_inside(in vec3 pos, in float mi, in float mx)
  {
    return pos.x >= mi && pos.y >= mi && pos.z >= mi &&
      pos.x < mx && pos.y < mx && pos.z < mx;
  }

  bool pos2_inside(in vec2 pos, in float mi, in float mx)
  {
    return min(pos.x, pos.y) >= mi && max(pos.x, pos.y) < mx;
  }

  bool pos2_inside_2(in vec2 pos, in vec2 mi, in vec2 mx)
  {
    return pos.x >= mi.x && pos.y >= mi.y &&
      pos.x < mx.x && pos.y < mx.y;
  }

/*
  vec3 voxel_next_noclamp(inout vec3 pos_f, in vec3 d)
  {
    vec3 r = vec3(0.0);
    float dzpos = float(d.z > 0.0);
    float zdelta = dzpos - pos_f.z;
    vec2 xy = pos_f.xy + d.xy * zdelta / d.z;
    if (d.z != 0.0 && pos2_inside(xy, 0.0, 1.0)) {
      r.z = d.z > 0.0 ? 1.0 : -1.0;
      pos_f = vec3(xy, dzpos);
      return r;
    }
    float dxpos = float(d.x > 0.0);
    float xdelta = dxpos - pos_f.x;
    vec2 yz = pos_f.yz + d.yz * xdelta / d.x;
    if (d.x != 0.0 && pos2_inside(yz, 0.0, 1.0)) {
      r.x = d.x > 0.0 ? 1.0 : -1.0;
      pos_f = vec3(dxpos, yz.xy);
      return r;
    }
    float dypos = float(d.y > 0.0);
    float ydelta = dypos - pos_f.y;
    vec2 zx = pos_f.zx + d.zx * ydelta / d.y;
    {
      r.y = d.y > 0.0 ? 1.0 : -1.0;
      pos_f = vec3(zx.y, dypos, zx.x);
      return r;
    }
  }
*/

  vec3 voxel_next_noclamp(inout vec3 pos_f, in vec3 d)
  {
    vec3 pos_i = vec3(0.0);
    vec3 spmin = vec3(0.0);
    vec3 spmax = vec3(1.0 - epsilon);
    vec3 r = vec3(0.0);
    float dzpos = d.z > 0.0 ? spmax.z : spmin.z;
    float zdelta = dzpos - pos_f.z;
    vec2 xy = pos_f.xy + d.xy * zdelta / d.z;
    if (d.z != 0.0 && pos2_inside_2(xy, spmin.xy, spmax.xy)) {
      r.z = d.z > 0.0 ? 1.0 : -1.0;
      vec3 npos = vec3(xy, dzpos);
      npos = clamp(npos, spmin, spmax);
      npos += pos_i;
      pos_i = floor(npos);
      pos_f = npos; //  - pos_i;
      return r;
    }
    float dxpos = d.x > 0.0 ? spmax.x : spmin.x;
    float xdelta = dxpos - pos_f.x;
    vec2 yz = pos_f.yz + d.yz * xdelta / d.x;
    if (d.x != 0.0 && pos2_inside_2(yz, spmin.yz, spmax.yz)) {
      r.x = d.x > 0.0 ? 1.0 : -1.0;
      vec3 npos = vec3(dxpos, yz.xy);
      npos = clamp(npos, spmin, spmax);
      npos += pos_i;
      pos_i = floor(npos);
      pos_f = npos; //  - pos_i;
      return r;
    }
    float dypos = d.y > 0.0 ? spmax.y : spmin.y;
    float ydelta = dypos - pos_f.y;
    vec2 zx = pos_f.zx + d.zx * ydelta / d.y;
    {
      r.y = d.y > 0.0 ? 1.0 : -1.0;
      vec3 npos = vec3(zx.y, dypos, zx.x);
      npos = clamp(npos, spmin, spmax);
      npos += pos_i;
      pos_i = floor(npos);
      pos_f = npos; //  - pos_i;
      return r;
    }
  }

  vec3 voxel_next(inout vec3 pos_i, inout vec3 pos_f, in vec3 spmin,
    in vec3 spmax, in vec3 d)
  {
    vec3 r = vec3(0.0);
    float dzpos = d.z > 0.0 ? spmax.z : spmin.z;
    float zdelta = dzpos - pos_f.z;
    vec2 xy = pos_f.xy + d.xy * zdelta / d.z;
    if (d.z != 0.0 && pos2_inside_2(xy, spmin.xy, spmax.xy)) {
      r.z = d.z > 0.0 ? 1.0 : -1.0;
      vec3 npos = vec3(xy, dzpos);
      npos = clamp(npos, spmin, spmax - epsilon);
      npos += pos_i;
      pos_i = floor(npos);
      pos_f = npos - pos_i;
      return r;
    }
    float dxpos = d.x > 0.0 ? spmax.x : spmin.x;
    float xdelta = dxpos - pos_f.x;
    vec2 yz = pos_f.yz + d.yz * xdelta / d.x;
    if (d.x != 0.0 && pos2_inside_2(yz, spmin.yz, spmax.yz)) {
      r.x = d.x > 0.0 ? 1.0 : -1.0;
      vec3 npos = vec3(dxpos, yz.xy);
      npos = clamp(npos, spmin, spmax - epsilon);
      npos += pos_i;
      pos_i = floor(npos);
      pos_f = npos - pos_i;
      return r;
    }
    float dypos = d.y > 0.0 ? spmax.y : spmin.y;
    float ydelta = dypos - pos_f.y;
    vec2 zx = pos_f.zx + d.zx * ydelta / d.y;
    {
      r.y = d.y > 0.0 ? 1.0 : -1.0;
      vec3 npos = vec3(zx.y, dypos, zx.x);
      npos = clamp(npos, spmin, spmax - epsilon);
      npos += pos_i;
      pos_i = floor(npos);
      pos_f = npos - pos_i;
      return r;
    }
  }

  bool raycast_octree(inout vec3 pos, in vec3 roottexpos, in vec3 eye,
    in vec3 light, out vec4 value_r, out vec3 dir_r, inout float lstr_para,
    inout vec4 dbgval)
  {
    bool hit = false;
    value_r = vec4(0.0);
    dir_r = vec3(0.0);
    vec3 ray = eye;
    const float block_factor = <%octree_block_factor/>;
    const float block_scale = 1.0 / block_factor;
    const vec3 texture_scale = <%octree_texture_scale/>;
    const int level_max = 3;
    vec3 texpos_arr[level_max];
      // 整数値を取る。テクスチャ座標(をテクスチャのサイズで乗じたもの)。
      // xyz座標をそれぞれblock_factorで割った余りがブロック内位置になる。
    int level = 0;
      // 再帰レベル
    vec3 curpos_t = roottexpos;
      // 現在見ているノード
    vec3 curpos_i = floor(pos * block_factor);
    vec3 curpos_f = (pos * block_factor) - curpos_i;
      // 現在見ているブロック内位置の整数部と小数部。整数部はblock_factor未満
    vec4 value;
    vec3 dir = vec3(0.0); // 最初の位置が接触していれば0をそのまま返す
    int i;
    for (i = 0; i < 128; ++i) {
      vec3 coord = curpos_t + curpos_i;
      value = <%texture3d/>(sampler_voxpat, coord * texture_scale);
      int node_type = int(floor(value.a * 255.0 + 0.5));
      if (node_type > 1) { // 色のついた葉
	if (hit) {
	  lstr_para = 0.0;
	  break;
	}
	value_r = value;
	dir_r = - dir;
	{
	  // 衝突した座標を計算(必要？)
	  vec3 hpos_t = curpos_t;
	  vec3 hpos_i = curpos_i;
	  vec3 hpos_f = curpos_f;
	  int hl = level;
	  while (hl > 0) {
	    --hl;
	    hpos_f = (hpos_i + hpos_f) * block_scale;
	    vec3 parent = texpos_arr[hl];
	    hpos_t = floor(parent * block_scale) * block_factor;
	    hpos_i = parent - hpos_t;
	  }
	  pos = (hpos_i + hpos_f) * block_scale;
	}
	hit = true;
	dir = -dir;
	float cos_light_dir = dot(light, dir);
	lstr_para = clamp(cos_light_dir * 64.0 - 1.0, 0.0, 1.0);
	if (lstr_para <= 0.0) {
	  break;
	}
	ray = light;
	//
	while (true) {
	  if (pos3_inside(curpos_i + dir, -0.5, block_factor - 0.5)) {
	    break;
	  }
	  --level;
	  if (level < 0) {
	    break;
	  }
	  curpos_f = (curpos_i + curpos_f) * block_scale;
	  vec3 parent = texpos_arr[level];
	  curpos_t = floor(parent * block_scale) * block_factor;
	  curpos_i = parent - curpos_t;
	}
	if (level < 0) {
	  break;
	}
	curpos_i += dir;
	curpos_f -= dir;
	//
      } else if (node_type == 1) { // 節
	texpos_arr[level] = coord;
	++level;
	curpos_t = floor(value.rgb * 255.0 + 0.5) * block_factor;
	curpos_f = clamp(curpos_f, 0.0, 1.0 - epsilon);
	curpos_i = floor(curpos_f * block_factor);
	curpos_f = (curpos_f * block_factor) - curpos_i;
      } else { // 空白 node_type == 0
	curpos_f = clamp(curpos_f, 0.0, 1.0 - epsilon);
	vec3 distval = floor(value.xyz * 255.0 + 0.5);
	vec3 dist_p = floor(distval / 16.0);
	vec3 dist_n = distval - dist_p * 16.0;
	vec3 spmin = 0.0 - dist_n;
	vec3 spmax = 1.0 + dist_p;
	dir = voxel_next(curpos_i, curpos_f, spmin, spmax, ray);
	while (true) {
	  if (pos3_inside(curpos_i + dir, -0.5, block_factor - 0.5)) {
	    break;
	  }
	  --level;
	  if (level < 0) {
	    break;
	  }
	  curpos_f = (curpos_i + curpos_f) * block_scale;
	  vec3 parent = texpos_arr[level];
	  curpos_t = floor(parent * block_scale) * block_factor;
	  curpos_i = parent - curpos_t;
	}
	if (level < 0) {
	  break;
	}
	curpos_i += dir;
	curpos_f -= dir;
      }
    }
    // dbgval.r = float(i) / 16.0;
    // dbgval.g = float(i) / 64.0;
    // dbgval.b = float(i) / 256.0;
    return hit;
  }

<%/>

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
   inout vec3 tsub, inout vec4 dbg)
  {
    tsub.xy = clamp(tsub.xy, 0.001, 0.999);
    dir /= max(abs(dir.x), abs(dir.y)); // FIXME: compute by caller?
      // W = unused(8), Z = depth(8), Y = CNN(4) CNP(4), X = CPN(4) CPP(4)
    float cvt = floor((dir.x > 0.0 ? tval.x : tval.y) * 255.0 + 0.5);
    float cval = dir.y > 0.0
      ? fract(cvt / 16.0) * 16.0
      : floor(cvt / 16.0);
    cval = min(cval, max(0.0, (tval.z - tsub.z - 0.001) / dir.z)); // crop cval
    // if (cval > 3.0) { dbg.r = 1.0; } // FIXME
    // cval = 0; // FIXME
    // if (dir.x <= 0.0 && dir.y > 0.0) { cval = 0; } // FIXME
    vec3 delta = clamp_to_border(tpos + tsub.xy, dir * cval, dbg);
    // vec3 delta = dir * cval; // FIXME
    vec2 npos = tpos + tsub.xy + delta.xy;
    tpos = floor(npos);
    tsub = vec3(npos - tpos, tsub.z + delta.z);
    parallax_read(tpos, tval);
  }

  void parallax_move_nozcrop(in vec3 dir, inout vec4 tval, inout vec2 tpos,
   inout vec3 tsub, inout vec4 dbg)
  {
    tsub.xy = clamp(tsub.xy, 0.001, 0.999);
    dir /= max(abs(dir.x), abs(dir.y)); // FIXME: compute by caller?
    float cvt = floor((dir.x > 0.0 ? tval.x : tval.y) * 255.0 + 0.5);
    float cval = dir.y > 0.0
      ? fract(cvt / 16.0) * 16.0
      : floor(cvt / 16.0);
    // if (cval > 3.0) { dbg.g = 1.0; } // FIXME
    // cval = 0; // FIXME
    // cval = max(0.0, cval - 0.1); // FIXME??
    vec3 delta = clamp_to_border(tpos + tsub.xy, dir * cval, dbg);
    // vec3 delta = dir * cval; // FIXME
    vec2 npos = tpos + tsub.xy + delta.xy;
    tpos = floor(npos);
    tsub = vec3(npos - tpos, tsub.z + delta.z);
    parallax_read(tpos, tval);
  }

  void parallax_loop(inout vec2 pos, in vec3 eye, in vec3 light,
    out float z, out vec3 nor, out bool vertical, out float shval,
    inout vec4 dbg)
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
      parallax_move_zcrop(eye, tval, tpos, tsub, dbg);
      if (tval.z < tsub.z) { // vertical surface
	vertical = true;
	nor = vary_tangent * (-tnext_diff.x)
	  + vary_binormal * (-tnext_diff.y);
	break;
      }
      // if (i > 32) { dbg.g = 1.0; }
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
	  parallax_move_nozcrop(light, tval, tpos, tsub, dbg);
	  if (tval.z < tsub.z) {
	    shval = 0.0;
	    break;
	  }
	  // if (i > 32) { dbg.b = 1.0; }
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

vec3 light_all(in vec3 light_color, in vec3 mate_specular,
  in vec3 mate_diffuse, in float mate_alpha, in vec3 camera_dir,
  in vec3 light_dir, samplerCube samp, in vec3 nor, bool vertical)
{
  vec3 half_l_v = normalize(light_dir + camera_dir);
  float cos_l_h = clamp(dot(light_dir, half_l_v), 0.0, 1.0);
  float cos_n_h = clamp(dot(nor, half_l_v), 0.0, 1.0);
  float cos_n_l = clamp(dot(nor, light_dir), 0.0, 1.0);
  float cos_n_v = clamp(dot(nor, camera_dir), 0.0, 1.0);
  float cos_v_h = cos_l_h;
  vec3 light_sp_di = light_color * (
    light_specular_brdf(cos_l_h, cos_n_h, cos_n_v, cos_n_l, cos_v_h,
    mate_alpha, mate_specular) +
    light_diffuse(cos_n_l, mate_diffuse));
  vec3 reflection_vec = reflect(-camera_dir, nor);
  vec3 env = <%texture_cube/>(sampler_env, reflection_vec).xyz;
  env = env * env * reflection_fresnel(
    clamp(dot(nor, reflection_vec), 0.0, 1.0), mate_specular)
    * (vertical ? mate_specular : vec3(1.0, 1.0, 1.0))
    * clamp(1.0 - mate_alpha, 0.0, 1.0);
  return clamp(light_sp_di + env, 0.0, 1.0);
}

void main(void)
{
  vec4 color = vec4(0.0, 0.0, 0.0, 1.0);
  vec3 nor = vary_normal;
  vec3 rel_camera_pos = camera_pos - vary_position;
  vec3 camera_dir = normalize(rel_camera_pos);
  float is_front = float(dot(camera_dir, nor) > 0.0);
  float frag_distance = length(rel_camera_pos);
  float distbr = clamp(16.0 / frag_distance, 0.0, 1.0);
  float lstr = 1.0;
  float lstr_para = 1.0;
  float para_zval = 0.0;
  bool vertical = false;
  vec4 tex_val = vec4(0.5, 0.5, 0.5, 1.0);
<%if><%not><%eq><%opt/>0<%/><%/>
// FIXME
<%/>
  <%if><%not><%eq><%opt/>0<%/><%/>
    <%if><%is_gl3_or_gles3/>
      mat3 normal_matrix = mat3(vary_model_matrix);
    <%else/>
      mat4 mm = vary_model_matrix;
      mat3 normal_matrix = mat3(mm[0].xyz, mm[1].xyz, mm[2].xyz);
    <%/>
    vec3 camera_local = -camera_dir * normal_matrix;
    vec3 light_local = light_dir * normal_matrix;
    const float obj_scale = 1.0 / 64.0;
    vec3 pos = clamp(vary_position_local * obj_scale + 0.5, 0.0, 1.0 - epsilon);
    vec3 campos = vary_camerapos_local * obj_scale + 0.5;
    vec3 sur_nor = vec3(0.0);
    bool dbg_inside = false; // FIXME
    if (pos3_inside(campos, 0.0, 1.0)) {
      pos = campos;
    //<%fragcolor/> = vec4(1.0,1.0,0.0,1.0); return;
    // color += vec4(1.0,1.0,0.0,1.0);
      dbg_inside = true; // FIXME
    } else {
//      color += vec4(1.0,0.0,0.0,1.0); return; // FIXME
      sur_nor = voxel_next_noclamp(pos, -camera_local);
    //<%fragcolor/> = vec4(pos,1.0); return;
    // color.r += 1.0;
    }
    // color = vec4(pos.xyz, 1.0);
    vec4 dbgval = vec4(0.0,0.0,0.0,0.0);
    if (!raycast_octree(pos, vec3(0.0), camera_local, light_local, tex_val, nor,
      lstr_para, dbgval))
    {
      // <%fragcolor/> = dbgval; return;
      discard;
    }
    // if (dbg_inside) { tex_val.r += .5; } // FIXME
    // tex_val.a = 0.0; // FIXME?
    if (length(nor) < 0.1) {
      // <%fragcolor/> = vec4(1.0); return;
      // nor = vec3(1.0, 0.0, 0.0); // FIXME
      nor = sur_nor;
      // tex_val = vec4(0.3, 0.1, 0.2, 1.0);
      lstr_para = 1.0;
      // if (length(nor) < 0.1) { <%fragcolor/> = vec4(1.0); return; }
    }
    nor = normal_matrix * nor; // local to global
    // color += v_col;
    // FIXME
    // <%fragcolor/> = dbgval; return;
  <%elseif/><%enable_normalmapping/>
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
      vec4 dbgval = vec4(0.0,0.0,0.0,0.0);
      parallax_loop(uv0, eye_tan, light_tan, para_zval, nor, vertical,
	lstr_para, dbgval);
      color += dbgval; // FIXME
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
  <%/> // opt, enable_normalmapping
  <%if><%and><%eq><%opt/>0<%/><%enable_shadowmapping/><%/>
    <%if><%light_fixed/>
      vec3 ndelta = vary_normal * ndelta_scale; // 0.02 / 40.
      vec3 prel = vary_position - camera_pos;
      vec3 p[<%smsz/>];
      <%variable den>
	<%set den><%shadowmap_distance/><%/>
	<%for i 0><%num_shadowmaps/>
	  p[<%i/>] = prel / <%den/>. + ndelta;
	  <%set den><%mul><%den/><%shadowmap_scale/><%/><%/>
	<%/>
      <%/>
    <%else/>
      <%if><%is_gl3_or_gles3/>
	vec3 p[<%smsz/>] = vary_smposa;
      <%else/>
	vec3 p[<%smsz/>];
	<%for i 0><%num_shadowmaps/>
	  p[<%i/>] = vary_smposa[<%i/>];
	<%/>
      <%/>
    <%/>
    <%for x 0><%num_shadowmaps/>
      <%if><%enable_depth_texture/>
	<%if><%enable_shadowmapping_multisample/>
	  <%if><%eq><%x/>0<%/>
	    vec3 smpos;
	  <%/>
	  smpos = (p[<%x/>] + 1.0) * 0.5;
	  float zval<%x/> = 0.0;
	  float zval<%x/>_cur = 0.0;
	  float sml<%x/> = 0.0;
	  for (float i = -1.; i <= 1.; ++i) {
	    for (float j = -1.; j <= 1.; ++j) {
	      zval<%x/>_cur = <%texture2d/>
		(sampler_sm[<%x/>], smpos.xy + vec2(i,j)/4096.0).x;
	      zval<%x/> = min(zval<%x/>, zval<%x/>_cur);
	      sml<%x/> += float(smpos.z < zval<%x/>_cur
	       * (1.0005 + (abs(i)+abs(j))/4096.0))/9.0;
	    }
	  }
	<%else/>
	  // no multisample
	  <%if><%eq><%x/>0<%/>
	    vec3 smpos;
	  <%/>
	  smpos = (p[<%x/>] + 1.0) * 0.5;
	  float zval<%x/> = <%texture2d/>
	    (sampler_sm[<%x/>], smpos.xy).x;
	  float sml<%x/> = float(smpos.z < zval<%x/> * 1.0005);
	<%/>
      <%elseif/><%enable_vsm/>
	// vsm, no depth texture
	<%if><%eq><%x/>0<%/>
	  vec3 smpos;
	  vec2 smz;
	  float dist;
	  float variance;
	<%/>
	<%if><%enable_shadowmapping_multisample/>
	  smpos = (p[<%x/>] + 1.0) * 0.5;
	  float zval<%x/> = 99999.0;
	  float sml<%x/> = 0.0;
	  for (float i = -1.0; i <= 1.0; i += 2.0) {
	    for (float j = -1.0; j <= 1.0; j += 2.0) {
	      smz = <%texture2d/>
		(sampler_sm[<%x/>], smpos.xy + vec2(i, j) / 8192.0).rg;
	      zval<%x/> = min(zval<%x/>, smz.r);
	      dist = smpos.z - smz.r * 1.001;
	      if (dist <= 0.0) {
	      sml<%x/> += 0.25;
	      } else {
	      variance = smz.g - smz.r * smz.r;
	      variance = max(variance, 0.000001);
	      sml<%x/> += 0.25 * variance / (variance + dist * dist);
	      }
	    } // for (j)
	  } // for (i)
	  sml<%x/> = clamp(sml<%x/> * 1.5 - 0.1, 0.0, 1.0);
	<%else/>
	  // no multisample
	  smpos = (p[<%x/>] + 1.0) * 0.5;
	  smz = <%texture2d/>
	    (sampler_sm[<%x/>], smpos.xy).rg;
	  float zval<%x/> = smz.r;
	  float zval<%x/>_sq = smz.g;
	  float sml<%x/> = 0.0;
	  dist = smpos.z - smz.r * 1.001;
	  if (dist <= 0.0) {
	  sml<%x/> = 1.0;
	  } else {
	  variance = smz.g - smz.r * smz.r;
	  variance = max(variance, 0.000001);
	  sml<%x/> = clamp(
	   variance / (variance + dist * dist) * 1.5 - 0.1, 0.0, 1.0);
	  }
	<%/>
      <%else/>
	// no depth texture, no vsm
	<%if><%eq><%x/>0<%/>
	  vec3 smpos;
	  vec3 smz;
	<%/>
	smpos = (p[<%x/>] + 1.0) * 0.5;
	smz = <%texture2d/>
	    (sampler_sm[<%x/>], smpos.xy).rgb;
	smz = floor(smz * 255.0 + 0.5);
	float zval<%x/> = 
	  smz.r / 256. + smz.g / 65536.0 + smz.b / 16777216.;
	float sml<%x/> = float(smpos.z < zval<%x/> * 1.0005);
      <%/>
    <%/>
    <%if><%enable_vsm/>
      const float zval_thr = 0.1;
    <%else/>
      const float zval_thr = <%fdiv>0.97f<%shadowmap_scale/><%/>;
    <%/>
    const float psm_thr = <%fdiv>0.9f<%shadowmap_scale/><%/>;
    if (zval0 < 0.0) { color.g += 1.0; }; // FIXME
    float smv0 = min(1.0, sml0
      + linear_01(max_vec3(abs(p[0])), 0.9, 1.0));
    lstr = min(lstr, smv0);
    <%for x 1><%num_shadowmaps/>
      float smv<%x/> = max3(sml<%x/>,
	min(linear_10(max_vec3(abs(p[<%x/>])), psm_thr * 0.9, psm_thr),
	linear_10(abs(zval<%x/>-0.5)*2.0, zval_thr * 0.9, zval_thr)),
	linear_01(max_vec3(abs(p[<%x/>])), 0.9, 1.0));
      lstr = min(lstr, smv<%x/>);
    <%/>
    // FIXME?
    lstr = clamp(dot(vary_normal, light_dir) * 4.0, 0.0, lstr);
    // lstr = clamp(dot(vary_normal, light_dir) * 32.0, 0.0, lstr);
    // lstr = min(lstr, float(dot(vary_normal, light_dir) > 0.0));
  <%/> // opt==0 enable_shadowmapping
  // fresnel
  // float cos_v_h = clamp(dot(camera_dir, half_l_v), 0.0, 1.0);
  float mate_alpha = 0.03;
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
  vec3 light_color2 = vec3(1.0, 1.0, 1.0) * lstr; // FIXME???
  vec3 light_color1 = light_color2 * lstr_para;
  vec3 li1 = light_all(light_color1, mate_specular, mate_diffuse,
    mate_alpha, camera_dir, light_dir, sampler_env, nor, vertical);
  /*
  vec3 li2 = light_all(light_color2, mate_specular, mate_diffuse,
    mate_alpha, camera_dir, light_dir, sampler_env,
    normalize(vary_normal), false);
  color.xyz += sqrt(mix(li2, li1, distbr));
  */
  color.xyz += sqrt(li1);
<%if><%not><%eq><%opt/>0<%/><%/>
// color.xyz = vary_uvw;
// color.xyz = vec3(1.0, 0.0, 0.0);
// FIXME
<%/>
  <%fragcolor/> = color;
}

