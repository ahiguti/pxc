
<%if><%eq><%get_config dbgval/>1<%/>
vec4 dbgval = vec4(0.0);
<%/>

const float epsilon = 1e-6;

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

vec3 div_rem(inout vec3 x, float y)
{
  vec3 r = floor(x / y);
  x -= r * y;
  return r;
}

bool pos3_inside(in vec3 pos, in float mi, in float mx)
{
  return pos.x >= mi && pos.y >= mi && pos.z >= mi &&
    pos.x < mx && pos.y < mx && pos.z < mx;
}

bool pos3_inside_3(in vec3 pos, in vec3 mi, in vec3 mx)
{
  return pos.x >= mi.x && pos.y >= mi.y && pos.z >= mi.z &&
    pos.x < mx.x && pos.y < mx.y && pos.z < mx.z;
}

bool pos3_inside_3_ge_le(in vec3 pos, in vec3 mi, in vec3 mx)
{
  return pos.x >= mi.x && pos.y >= mi.y && pos.z >= mi.z &&
    pos.x <= mx.x && pos.y <= mx.y && pos.z <= mx.z;
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
  /*
  float dxpos = dpos.x;
  float xdelta = delta.x;
  vec2 yz = vec2(c0.x, c1.x);
  // float dxpos = d.x > 0.0 ? spmax.x : spmin.x;
  // float xdelta = dxpos - pos_f.x;
  // vec2 yz = pos_f.yz + d.yz * xdelta / d.x;
  if (d.x != 0.0 && pos2_inside_2(yz, spmin.yz, spmax.yz)) {
    r.x = d.x > 0.0 ? 1.0 : -1.0;
    vec3 npos = vec3(dxpos, yz.xy);
    npos = clamp(npos, spmin, spmax - epsi);
    npos += pos_i;
    pos_i = floor(npos);
    pos_f = npos - pos_i;
    return r;
  }
  float dypos = dpos.y;
  float ydelta = delta.y;
  vec2 zx = vec2(c0.y, c1.y);
  // float dypos = d.y > 0.0 ? spmax.y : spmin.y;
  // float ydelta = dypos - pos_f.y;
  // vec2 zx = pos_f.zx + d.zx * ydelta / d.y;
  if (d.y != 0.0 && pos2_inside_2(zx, spmin.zx, spmax.zx)) {
    r.y = d.y > 0.0 ? 1.0 : -1.0;
    vec3 npos = vec3(zx.y, dypos, zx.x);
    npos = clamp(npos, spmin, spmax - epsi);
    npos += pos_i;
    pos_i = floor(npos);
    pos_f = npos - pos_i;
    return r;
  }
  float dzpos = dpos.z;
  float zdelta = delta.z;
  vec2 xy = vec2(c0.z, c1.z);
  // float dzpos = d.z > 0.0 ? spmax.z : spmin.z;
  // float zdelta = dzpos - pos_f.z;
  // vec2 xy = pos_f.xy + d.xy * zdelta / d.z;
  // if (d.z != 0.0 && pos2_inside_2(xy, spmin.xy, spmax.xy))
  {
    r.z = d.z > 0.0 ? 1.0 : -1.0;
    vec3 npos = vec3(xy, dzpos);
    npos = clamp(npos, spmin, spmax - epsi);
    npos += pos_i;
    pos_i = floor(npos);
    pos_f = npos - pos_i;
    return r;
  }
  */
}

float voxel_collision_sphere(in vec3 v, in vec3 a, in vec3 c,
  in vec3 mul_pt, in float rad2_pt, in bool ura, out bool hit_wall_r,
  out vec3 nor_r)
{
// FIXME: 裏返しテスト中
// ura = true;
  hit_wall_r = false;
  nor_r = vec3(0.0);
  // vはray単位ベクトル, aは始点(-0.5,0.5)範囲, mul_ptはaからの拡大率,
  // c_ptは球の中心座標, rad2_ptは球の半径の2乗,
  // hit_wallは開始点で衝突していればtrue,
  // 返値len_aeは交点までの距離, 交点は a + v * len_aeで求まる
  vec3 a_pt = mul_pt * a;
  vec3 v_pt = mul_pt * v;
  vec3 c_pt = c;
  float len_v_pt = length(v_pt);
  vec3 v_ptn = normalize(v_pt);
  vec3 ac_pt = c_pt - a_pt; // 始点aから球の中心c
  float len2_ac_pt = dot(ac_pt, ac_pt);
  if ((len2_ac_pt > rad2_pt) == ura) {
    hit_wall_r = true; // 始点がすでに球の内側
    return 0.0;
  }
  float ac_v_pt = dot(ac_pt, v_ptn);
  vec3 d_pt = a_pt + v_ptn * ac_v_pt; // cから視線上に垂線をおろしたた点
  vec3 cd_pt = d_pt - c_pt;
  float len2_cd_pt = dot(cd_pt, cd_pt);
  if (len2_cd_pt > rad2_pt) {
    return 256.0; // 球と直線は接触しない
  }
  <%if><%eq><%get_config dbgval/>1<%/>
  // dbgval = vec4(1.0); hit_wall_r = true; return 0.0;
  <%/>
  float len_de_pt = sqrt(rad2_pt - len2_cd_pt);
  float len_ae_pt = ac_v_pt + (ura ? len_de_pt : -len_de_pt);
  vec3 e_pt = a_pt + v_ptn * len_ae_pt; // 視線が球面と接触する点
  vec3 ce_pt = e_pt - c_pt;
  nor_r = normalize(ce_pt / max(mul_pt, 0.125)); // 0除算しない
  if (ura) {
    nor_r = -nor_r;
  }
  float len_ae = len_ae_pt / len_v_pt;
  return len_ae;
}

<%if><%eq><%stype/>1<%/>

const vec3 tile3_size = vec3(<%tile3_size/>);
const int tile3_size_log2 = <%tile3_size_log2/>.x;
const vec3 pat3_size = vec3(<%pat3_size/>);
const vec3 pattex3_size = tile3_size * pat3_size;
const vec3 map3_size = vec3(<%map3_size/>);
const int map3_size_log2 = <%map3_size_log2/>.x;
const vec3 virt3_size = vec3(<%virt3_size/>);
const int virt3_size_log2 = tile3_size_log2 + map3_size_log2;

uniform <%mediump_sampler3d/> sampler_voxtpat;
uniform <%mediump_sampler3d/> sampler_voxtpax;
uniform <%mediump_sampler3d/> sampler_voxtmap;
uniform <%mediump_sampler3d/> sampler_voxtmax;

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

int tilemap_fetch_debug(in vec3 pos, int tmap_mip, int tpat_mip)
{
  // float distance_unit = distance_unit_tmap_mip;
  vec3 curpos_f = pos * virt3_size + vec3(0.5);
  vec3 curpos_i = div_rem(curpos_f, 1.0);
  vec3 curpos_t = floor(curpos_i / tile3_size);
  vec3 curpos_tr = curpos_i - curpos_t * tile3_size; // 0から15の整数
  <%if><%is_gl3_or_gles3/>
  ivec3 icp = ivec3(curpos_t);
  vec4 value = texelFetch(sampler_voxtmap, icp, 0);
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

int raycast_waffle(inout vec3 pos, in vec3 fragpos, in vec3 ray,
  in vec3 mi, in vec3 mx, in int miplevel)
{
  // 引数の座標はすべてテクスチャ座標
  // TODO: 速くする余地あり
  int tmap_mip = clamp(miplevel - tile3_size_log2, 0, tile3_size_log2);
  int tpat_mip = min(miplevel, tile3_size_log2);
  float dist_max = length(pos - fragpos);
  float di = 2.0;
  float near = 65536.0;
  if (true) { // どっちが速い？
    vec3 d = (mx - mi) / di;
    vec3 dd = d * 0.5 / vec3(virt3_size); // FIXME????
      // voxel境界付近を拾わないようにするために少しずらす
    mi = mi + dd;
    mx = mx - dd;
    d = (mx - mi) / di;
    vec3 ad = d / ray;
    bvec3 ad_nega = lessThan(ad, vec3(0.0));
    vec3 f = mi + d * (0.5 + vec3(ad_nega) * (di - 1.0));
      // adが正ならmiから0.5, 負ならmxから0.5
    ad = abs(ad);
    vec3 a = (f - pos) / ray;
    for (float i = 0.0; i < di; i = i + 1.0, a.x = a.x + ad.x) {
      if (a.x > 0.0 && a.x < dist_max) {
//vec3 p = pos + ray * a.x; if (!pos3_inside_3(p, mi, mx)) { break; }
	if (tilemap_fetch(pos + ray * a.x, tmap_mip, tpat_mip) == 255) {
	  near = min(a.x, near);
	  break;
	}
      }
    }
    for (float i = 0.0; i < di; i = i + 1.0, a.y = a.y + ad.y) {
      if (a.y > 0.0 && a.y < dist_max) {
//vec3 p = pos + ray * a.y; if (!pos3_inside_3(p, mi, mx)) { break; }
	if (tilemap_fetch(pos + ray * a.y, tmap_mip, tpat_mip) == 255) {
	  near = min(a.y, near);
	  break;
	}
      }
    }
    for (float i = 0.0; i < di; i = i + 1.0, a.z = a.z + ad.z) {
      if (a.z > 0.0 && a.z < dist_max) {
//vec3 p = pos + ray * a.z; if (!pos3_inside_3(p, mi, mx)) { break; }
	if (tilemap_fetch(pos + ray * a.z, tmap_mip, tpat_mip) == 255) {
	  near = min(a.z, near);
	  break;
	}
      }
    }
  } else {
    vec3 d = (mx - mi) / di;
    vec3 f = mi + d * 0.5; // + epsilon;
    vec3 ad = d / ray;
    vec3 a = (f - pos) / ray;
    for (float i = 0.0; i < di; i = i + 1.0, a = a + ad) {
      if (a.x > 0.0 && a.x < dist_max) {
//vec3 p = pos + ray * a.x; if (!pos3_inside_3(p, mi, mx)) { break; }
	// if (tilemap_fetch(pos + ray * a.x, tmap_mip, tpat_mip) == 255) {
	if (tilemap_fetch(pos + ray * a.x, tmap_mip, tpat_mip) == 255) {
	  near = min(a.x, near);
	}
      }
      if (a.y > 0.0 && a.y < dist_max) {
//vec3 p = pos + ray * a.y; if (!pos3_inside_3(p, mi, mx)) { break; }
	// if (tilemap_fetch(pos + ray * a.y, tmap_mip, tpat_mip) == 255) {
	if (tilemap_fetch(pos + ray * a.y, tmap_mip, tpat_mip) == 255) {
//if (p.z < 0.0001) break;
	  near = min(a.y, near);
	}
      }
      if (a.z > 0.0 && a.z < dist_max) {
//tmap_mip = 0;
//tpat_mip = 0;
//vec3 p = pos + ray * a.z; if (!pos3_inside_3(p, mi, mx)) { break; }
	// if (tilemap_fetch(pos + ray * a.z, tmap_mip, tpat_mip) == 255) {
	if (tilemap_fetch(pos + ray * a.z, tmap_mip, tpat_mip) == 255) {
//if (p.y >= 0.9) break;
//break;
	  near = min(a.z, near);
	}
      }
    }
  }
  // return near < 65535 ? near : -1.0f;
  if (near < 65535.0) {
    pos = pos + ray * near;
    return 1;
  } else {
    return -1;
  } // elseの中に入れないとnvidiaのバグに当たる
}

int raycast_get_miplevel(in vec3 pos, in vec3 campos, in float dist_rnd)
{
  // テクスチャ座標でのposとcamposからmiplevelを決める
  float dist_pos_campos_2 = dot(pos - campos, pos - campos) + 0.0001;
  float dist_log2 = log(dist_pos_campos_2) * 0.5 / log(2.0);
  /// return int(dist_log2 + dist_rnd * 4.0 + float(virt3_size_log2) - 9.5);
  // return int(dist_log2 * 1.0 + dist_rnd * 4.0 + float(virt3_size_log2) - 9.5);
  return int(dist_log2 * 1.0 + dist_rnd * 4.0 + float(virt3_size_log2) - 9.0);
  // return int(dist_log2 * 1.0 + dist_rnd * 4.0 + float(virt3_size_log2) - 7.5);
    // TODO: LODバイアス調整できるようにする
}

vec3 tpat_sgn_rotate_tile(in vec3 value, in vec3 rot, in vec3 sgn)
{
  // rotとsgnを適用してtpat座標を返す
  float e = 1.0; // / 65536.0; // valueは整数なので1.0でよい
  if (sgn.x < 0.0) { value.x = tile3_size.x - e - value.x; }
  if (sgn.y < 0.0) { value.y = tile3_size.y - e - value.y; }
  if (sgn.z < 0.0) { value.z = tile3_size.z - e - value.z; }
  if (rot.x != 0.0) { value.xy = value.yx; }
  if (rot.y != 0.0) { value.yz = value.zy; }
  if (rot.z != 0.0) { value.zx = value.xz; }
  return value;
}

vec3 tpat_rotate_distval(in vec3 value, in vec3 rot)
{
  if (rot.z != 0.0) { value.zx = value.xz; }
  if (rot.y != 0.0) { value.yz = value.zy; }
  if (rot.x != 0.0) { value.xy = value.yx; }
  return value;
}

void swap_float(inout float a, inout float b)
{
  float t = b;
  b = a;
  a = t;
}

void tpat_sgn_distval(in vec3 i_p, in vec3 i_n, in vec3 sgn, out vec3 o_p,
  out vec3 o_n)
{
  o_p = i_p;
  o_n = i_n;
  if (sgn.x < 0) { swap_float(o_p.x, o_n.x); }
  if (sgn.y < 0) { swap_float(o_p.y, o_n.y); }
  if (sgn.z < 0) { swap_float(o_p.z, o_n.z); }
}

int raycast_tilemap(
  inout vec3 pos, in vec3 campos, in float dist_rand,
  in vec3 eye, in vec3 light,
  in vec3 aabb_min, in vec3 aabb_max, out vec4 value0_r, out vec4 value1_r,
  inout vec3 hit_nor,
  in float selfshadow_para, inout float lstr_para, inout int miplevel,
  in bool enable_variable_miplevel)
{
  // 引数の座標はすべてテクスチャ座標
  // eyeはカメラから物体への向き、lightは物体から光源への向き
  int miplevel0 = miplevel;
  bool mip_detail = false; // 詳細モードかどうか
  if (enable_variable_miplevel && max_vec3(aabb_max - aabb_min) > 0.125f) {
    // 長距離のイテレートを速くするために大きいmiplevelから開始する。
    // テクスチャに余白が無いと短冊状に影ができてしまう問題があるので
    // 大きいオブジェクトに限って適用する。
    miplevel = max(miplevel0, 6);
    mip_detail = miplevel0 == miplevel;
  }
  int tmap_mip = clamp(miplevel - tile3_size_log2, 0, tile3_size_log2);
  int tpat_mip = min(miplevel, tile3_size_log2);
  float distance_unit_tmap_mip = float(<%lshift>16<%>tmap_mip<%/>);
  float distance_unit_tpat_mip = float(<%lshift>1<%>tpat_mip<%/>);
  // if (tpat_mip != 0) { dbgval = vec4(1.0, 1.0, 0.0, 1.0); }
  vec3 ray = eye;
  vec3 dir = -hit_nor;
  vec3 curpos_f = pos * virt3_size;
  vec3 curpos_i = div_rem(curpos_f, 1.0);
  value0_r = vec4(0.0, 0.0, 0.0, 1.0);
  value1_r = vec4(0.0, 0.0, 0.0, 1.0);
  int hit = -1;
  bool hit_tpat;
  vec3 hit_coord;
  vec4 hit_value = vec4(0.0);
  int node_type = 0;
  int i;
  const int imax = 64; // raycastループ回数の上限
  for (i = 0; i < imax; ++i) {
    if (mip_detail && hit < 0) {
      // 詳細モードであればカメラからの距離に応じたmiplevelでテクスチャを引く
      vec3 ppos = curpos_i / virt3_size;
      miplevel = clamp(raycast_get_miplevel(ppos, campos, dist_rand), 0, 8);
      tmap_mip = clamp(miplevel - tile3_size_log2, 0, tile3_size_log2);
      tpat_mip = min(miplevel, tile3_size_log2);
      distance_unit_tmap_mip = float(<%lshift>16<%>tmap_mip<%/>);
      distance_unit_tpat_mip = float(<%lshift>1<%>tpat_mip<%/>);
    }
    vec3 curpos_t = floor(curpos_i / tile3_size);
    vec3 curpos_tr = curpos_i - curpos_t * tile3_size; // 0から15の整数
    vec3 tmap_coord = curpos_t;
    <%if><%is_gl3_or_gles3/>
    vec4 value = texelFetch(sampler_voxtmap, ivec3(tmap_coord) >> tmap_mip, 
      tmap_mip);
    <%else/>
    vec4 value = <%texture3d/>(sampler_voxtmap, tmap_coord / map3_size);
    <%/>
    node_type = int(floor(value.a * 255.0 + 0.5));
    if (node_type == 255 && !mip_detail && enable_variable_miplevel) {
      // 詳細モードでなくてfilledと衝突したなら詳細モードに入る
      mip_detail = true;
      continue;
    }
    bool is_pat = (node_type == 1);
    float distance_unit = distance_unit_tmap_mip;
    vec3 tpat_rot = vec3(0.0);
    vec3 tpat_sgn = vec3(1.0);
    vec3 tpat_coord;
    if (is_pat) {
      distance_unit = 1.0;
      vec3 vp = floor(value.rgb * 255.0 + 0.5);
      tpat_rot = div_rem(vp, 128.0);
      tpat_sgn = 1.0 - div_rem(vp, 64.0) * 2.0; // -1 or +1
      // tpat_rot.y = 1.0; // FIXME
      // tpat_sgn.z = -1.0; // FIXME
      vec3 curpos_tp = vp * tile3_size; // タイルパターンの始点 16刻み4096迄
      distance_unit = distance_unit_tpat_mip;
      tpat_coord = curpos_tp + tpat_sgn_rotate_tile(curpos_tr, tpat_rot,
	tpat_sgn);
	// tpat回転: curpos_trはタイル内オフセット。
	// tile3_size未満の値。これをrotate_tile(curpos_tr, value.a)に
	// 置き換える。
      <%if><%is_gl3_or_gles3/>
      value = texelFetch(sampler_voxtpat, ivec3(tpat_coord) >> tpat_mip,
	tpat_mip);
      <%else/>
      value = <%texture3d/>(sampler_voxtpat, (tpat_coord) / pattex3_size);
      <%/>
      node_type = int(floor(value.a * 255.0 + 0.5));
    }
    // ボクセルの大きさを掛ける。タイルの移動なら16倍
    curpos_t = floor(curpos_i / distance_unit);
    curpos_tr = curpos_i - curpos_t * distance_unit;
    curpos_f = (curpos_tr + curpos_f) / distance_unit;
    curpos_f = clamp(curpos_f, 0.0, 1.0); // FIXME: 必要？
    curpos_i = curpos_t * distance_unit;
      // curpos_iだけはdistance_unit単位にはしない
      // 現在座標は curpos_i + curpos_f * distance_unit で求まる
    // 衝突判定
    vec3 spmin = vec3(0.0);
    vec3 spmax = vec3(1.0);
    vec3 distval = tpat_rotate_distval(floor(value.xyz * 255.0 + 0.5),
      tpat_rot); // tpat回転だけ適用済み。sgnは未適用
    vec3 distval_h = floor(distval / 16.0);
    vec3 distval_l = distval - distval_h * 16.0;
    vec3 dist_p;
    vec3 dist_n;
    tpat_sgn_distval(distval_h, distval_l, tpat_sgn, dist_p, dist_n);
    if (node_type == 0) { // 空白
      spmin = vec3(0.0) - dist_n;
      spmax = vec3(1.0) + dist_p;
      // if (dist_p.y > 14.5) { dbgval = vec4(1.0, 1.0, 0.0, 1.0); }
    } else {
      bool hit_flag = true;
      bool hit_wall = false;
      if (node_type == 255) { // 壁
	hit_wall = true;
      } else { // 平面または二次曲面で切断
	// node_type = 208 + 0; 
	// dist_p = vec3(9.0,9.0,9.0);
	vec3 sp_nor;
	float length_ae;
	if (node_type >= 160) {
	  // 平面
	  float param_d = float(node_type - 208); // -48, +46
	  vec3 param_abc = distval_h - 8.0; // distval_lは未使用
	  param_abc = param_abc * tpat_sgn; // tpat sgnを適用
	  vec3 coord = (curpos_f - 0.5) * 2.0;
	  float dot_abc_p = dot(param_abc, coord);
	  float pl = dot_abc_p - param_d; // 正なら現在位置では空白
	  hit_wall = pl > 0.0;
	  length_ae = (-pl) * 0.5 / dot(param_abc, ray);
	  sp_nor = -normalize(param_abc);
	} else {
	  // 楕円体
	  vec3 sp_scale = floor(distval / 64.0); // 上位2bit
	  vec3 sp_center = distval - sp_scale * 64.0 - 32.0; // 下位6bit
	  sp_center = sp_center * tpat_sgn; // tpat sgnを適用
	  float sp_radius = float(node_type - 1);
          bool ura = (node_type - 1 > 64);
          if (ura) {
            sp_radius -= 64.0;
          }
	  sp_nor = vec3(0.0);
	  length_ae = voxel_collision_sphere(ray, curpos_f - 0.5,
	    sp_center, sp_scale, sp_radius * sp_radius, ura, hit_wall, sp_nor);
	}
	vec3 tp = curpos_f + ray * length_ae;
	if (hit_wall) {
	} else if (!pos3_inside(tp, 0.0 - epsilon, 1.0 + epsilon)) {
	  // 断面と境界平面の境目の誤差を見えなくするためにepsilonだけ広げる
	  hit_flag = false;
	} else {
	  // ボクセル内で断面に接触
	  dir = -sp_nor;
	  curpos_f = tp;
	}
      }
      // miplevelが0でないときは初期miplevelでのraycastのめり込み対策
      // のためi == hit + 1のときは影にしない(TODO: テストケース)
      // is_tpatのときはその処理はしない(slit1のmiplevel2でテスト)
      if (hit_flag && (!is_pat || i != hit + 1 || hit < 0 || miplevel == 0)) {
	// 接触した
	if (hit >= 0) {
	  // lightが衝突したので影にする
	  lstr_para = lstr_para * selfshadow_para;
	  break;
	}
	hit_nor = -dir;
	hit = i;
	hit_tpat = is_pat;
	hit_coord = is_pat ? tpat_coord : tmap_coord;
        hit_value = value;
	pos = (curpos_i + curpos_f * distance_unit) / virt3_size;
	  // eyeが衝突した位置
	// 法線と光が逆向きのときは必ず影(陰)
	float cos_light_dir = dot(light, hit_nor);
	lstr_para = clamp(cos_light_dir * 64.0 - 1.0, 0.0, 1.0);
	if (lstr_para <= 0.0) {
	  break;
	}
	if (i == 0 && hit_wall) {
	  // raycast始点がすでに石の中にいるのでセルフシャドウは差さない
	  break;
	}
	// 影判定開始
	ray = light;
	spmin = vec3(0.0); // TODO: -dir方向へ一回移動するか
	spmax = vec3(1.0);
        // 裏返し球のときは影判定開始ボクセル内で衝突する可能性があるので、
        // ここで判定する。それ以外の形状についてはその可能性はない。
	if (node_type >= 2 + 64 && node_type < 160) {
          bool ura = true;
          bool light_hit_wall = false;
	  vec3 sp_scale = floor(distval / 64.0); // 上位2bit
	  vec3 sp_center = distval - sp_scale * 64.0 - 32.0; // 下位6bit
	  sp_center = sp_center * tpat_sgn; // tpat sgnを適用
	  float sp_radius = float(node_type - 64 - 1) * 1.0;
	  vec3 sp_nor = vec3(0.0);
          float length_ae;
          curpos_f = curpos_f + light * 0.01;
	  length_ae = voxel_collision_sphere(ray, curpos_f - 0.5,
	    sp_center, sp_scale, sp_radius * sp_radius, ura, light_hit_wall,
            sp_nor);
          vec3 tp = curpos_f + ray * length_ae;
          if (pos3_inside(tp, 0.0, 1.0)) {
            lstr_para = lstr_para * selfshadow_para;
            break;
          }
        }
      }
    }
    vec3 npos;
    dir = voxel_get_next(curpos_f, spmin, spmax, ray, npos);
    // if (dot(dir, ray) <= 0.0) { dbgval=vec4(1,1,1,1); return -1; }
    // npos = clamp(npos, spmin, spmax); // FIXME: ???
    npos *= distance_unit;
    vec3 npos_i = min(floor(npos), spmax * distance_unit - 1.0);
    /*
    vec3 npos_i;
    if (is_pat) {
      npos_i = min(floor(npos), spmax - 1.0); // ギリギリボクセル整数部分
    } else {
      npos *= 16.0;
      npos_i = min(floor(npos), spmax * 16.0 - 1.0);
    }
    */
    npos_i += dir; // 壁を突破
    npos = npos - npos_i; // 0, 1の範囲に収まっているはず
    // if (length(npos_i) < 0.1) {
      // dbgval=vec4(0.0,1.0,1.0,1.0);  return -1;
    // }
    curpos_i += npos_i;
    curpos_f = npos;
    bool is_inside_aabb = pos3_inside_3(curpos_i /* + curpos_f */,
      aabb_min * virt3_size, aabb_max * virt3_size);
    if (!is_inside_aabb) {
      break;
    }
  } // for
  if (i == imax) {
    // ループ上限に達した。eye計算中かlight計算中かは両方ありうる。
    lstr_para = 0.0;
    // hit = i; // なんでこの行有効だったのか？
  }
  if (hit >= 0) {
    if (!hit_tpat) {
      <%if><%is_gl3_or_gles3/>
      value1_r = texelFetch(sampler_voxtmax, ivec3(hit_coord) >> tmap_mip, 
	tmap_mip);
      <%else/>
      value1_r = <%texture3d/>(sampler_voxtmax, hit_coord / map3_size);
      <%/>
    } else {
      <%if><%is_gl3_or_gles3/>
      value1_r = texelFetch(sampler_voxtpax, ivec3(hit_coord) >> tpat_mip,
	tpat_mip);
      <%else/>
      value1_r = <%texture3d/>(sampler_voxtpax, (hit_coord) / pattex3_size);
      <%/>
    }
    int hit_node_type = int(floor(hit_value.a * 255.0 + 0.5));
    value0_r = hit_node_type == 255 ? hit_value : value1_r;
      // value0_rはemissionのrgb値を保持する。filledならprimaryから、それ以外
      // ならsecondaryの色をそのまま使う。
  }
  // if (i > 35) { dbgval = vec4(1.0, 1.0, 0.0, 1.0); }
  // if (hit > 32) { dbgval = vec4(1.0, 0.0, 0.0, 1.0); } // FIXME
  return hit;
}

// 旧バージョン
int raycast_tilemap_em(
  inout vec3 pos, in vec3 campos, in float dist_rand,
  in vec3 eye, in vec3 light,
  in vec3 aabb_min, in vec3 aabb_max, out vec4 value_r, inout vec3 hit_nor,
  in float selfshadow_para, inout float lstr_para, inout int miplevel,
  in bool enable_variable_miplevel)
{
  // 引数の座標はすべてテクスチャ座標
  // eyeはカメラから物体への向き、lightは物体から光源への向き
  int tmap_mip = clamp(miplevel - tile3_size_log2, 0, tile3_size_log2);
  int tpat_mip = min(miplevel, tile3_size_log2);
  float distance_unit_tmap_mip = float(<%lshift>16<%>tmap_mip<%/>);
  float distance_unit_tpat_mip = float(<%lshift>1<%>tpat_mip<%/>);
  vec3 ray = eye;
  vec3 dir = -hit_nor;
  vec3 curpos_f = pos * virt3_size;
  vec3 curpos_i = div_rem(curpos_f, 1.0);
  value_r = vec4(0.0, 0.0, 0.0, 1.0);
  int hit = -1;
  bool hit_tpat;
  vec3 hit_coord;
  int i;
  const int imax = 256;
  for (i = 0; i < imax; ++i) {
    vec3 curpos_t = floor(curpos_i / tile3_size);
    vec3 curpos_tr = curpos_i - curpos_t * tile3_size; // 0から15の整数
    vec3 tmap_coord = curpos_t;
    <%if><%is_gl3_or_gles3/>
    vec4 value = texelFetch(sampler_voxtmap, ivec3(tmap_coord) >> tmap_mip, 
      tmap_mip);
    <%else/>
    vec4 value = <%texture3d/>(sampler_voxtmap, tmap_coord / map3_size);
    <%/>
    int node_type = int(floor(value.a * 255.0 + 0.5));
    bool is_pat = (node_type == 1);
    float distance_unit = distance_unit_tmap_mip;
    vec3 tpat_coord;
    if (is_pat) {
      distance_unit = 1.0;
      vec3 curpos_tp = floor(value.rgb * 255.0 + 0.5) * tile3_size;
	// 16刻み4096迄
      distance_unit = distance_unit_tpat_mip;
      tpat_coord = curpos_tp + curpos_tr;
      <%if><%is_gl3_or_gles3/>
      value = texelFetch(sampler_voxtpat, ivec3(tpat_coord) >> tpat_mip,
	tpat_mip);
      <%else/>
      value = <%texture3d/>(sampler_voxtpat, (tpat_coord) / pattex3_size);
      <%/>
      node_type = int(floor(value.a * 255.0 + 0.5));
    }
    // ボクセルの大きさを掛ける。タイルの移動なら16倍
    curpos_t = floor(curpos_i / distance_unit);
    curpos_tr = curpos_i - curpos_t * distance_unit;
    curpos_f = (curpos_tr + curpos_f) / distance_unit;
    curpos_f = clamp(curpos_f, 0.0, 1.0); // FIXME: 必要？
    curpos_i = curpos_t * distance_unit;
      // curpos_iだけはdistance_unit単位にはしない
      // 現在座標は curpos_i + curpos_f * distance_unit で求まる
    // 衝突判定
    vec3 spmin = vec3(0.0);
    vec3 spmax = vec3(1.0);
    vec3 distval = floor(value.xyz * 255.0 + 0.5);
    vec3 dist_p = floor(distval / 16.0);
    vec3 dist_n = distval - dist_p * 16.0;
    if (node_type == 0) { // 空白
      spmin = vec3(0.0) - dist_n;
      spmax = vec3(1.0) + dist_p;
    } else {
      bool hit_flag = true;
      bool hit_wall = false;
      if (node_type == 255) { // 壁
	hit_wall = true;
      } else { // 平面または二次曲面で切断
	// node_type = 208 + 0; 
	// dist_p = vec3(9.0,9.0,9.0);
	vec3 sp_nor;
	float length_ae;
	if (node_type >= 160) {
	  // 平面
	  float param_d = float(node_type - 208); // -48, +46
	  vec3 param_abc = dist_p - 8.0; // dist_nは未使用
	  vec3 coord = (curpos_f - 0.5) * 2.0;
	  float dot_abc_p = dot(param_abc, coord);
	  float pl = dot_abc_p - param_d; // 正なら現在位置では空白
	  hit_wall = pl > 0.0;
	  length_ae = (-pl) * 0.5 / dot(param_abc, ray);
	  sp_nor = -normalize(param_abc);
	} else {
	  // 楕円体
	  vec3 sp_scale = floor(distval / 64.0); // 上位2bit
	  vec3 sp_center = distval - sp_scale * 64.0 - 32.0; // 下位6bit
	  // vec3 sp_scale = dist_p; // 拡大率
	  // vec3 sp_center = dist_n - 8.0; // 球の中心の相対位置
	  float sp_radius = float(node_type - 1) * 1.0;
	  sp_nor = vec3(0.0);
          // ura未対応
	  length_ae = voxel_collision_sphere(ray, curpos_f - 0.5,
	    sp_center, sp_scale, sp_radius * sp_radius, false, hit_wall,
            sp_nor);
	}
	vec3 tp = curpos_f + ray * length_ae;
	if (hit_wall) {
	} else if (!pos3_inside(tp, 0.0 - epsilon, 1.0 + epsilon)) {
	  // 断面と境界平面の境目の誤差を見えなくするためにepsilonだけ広げる
	  hit_flag = false;
	} else {
	  // ボクセル内で断面に接触
	  dir = -sp_nor;
	  curpos_f = tp;
	}
      }
      if (hit_flag) {
	// 接触した
	if (hit >= 0) {
	  // lightが衝突したので影にする
	  lstr_para = lstr_para * selfshadow_para;
	  break;
	}
	hit_nor = -dir;
	hit = i;
	hit_tpat = is_pat;
	hit_coord = is_pat ? tpat_coord : tmap_coord;
	pos = (curpos_i + curpos_f * distance_unit) / virt3_size;
	  // eyeが衝突した位置
	// 法線と光が逆向きのときは必ず影(陰)
	float cos_light_dir = dot(light, hit_nor);
	lstr_para = clamp(cos_light_dir * 64.0 - 1.0, 0.0, 1.0);
	if (lstr_para <= 0.0) {
	  break;
	}
	if (i == 0 && hit_wall) {
	  // raycast始点がすでに石の中にいるのでセルフシャドウは差さない
	  break;
	}
	// 影判定開始
	ray = light;
	spmin = vec3(0.0); // TODO: -dir方向へ一回移動するか
	spmax = vec3(1.0);
      }
    }
    vec3 npos;
    dir = voxel_get_next(curpos_f, spmin, spmax, ray, npos);
    // if (dot(dir, ray) <= 0.0) { dbgval=vec4(1,1,1,1); return -1; }
    // npos = clamp(npos, spmin, spmax); // FIXME: ???
    npos *= distance_unit;
    vec3 npos_i = min(floor(npos), spmax * distance_unit - 1.0);
    // vec3 npos_i;
    // if (is_pat) {
    //   npos_i = min(floor(npos), spmax - 1.0); // ギリギリボクセル整数部分
    // } else {
    //   npos *= 16.0;
    //   npos_i = min(floor(npos), spmax * 16.0 - 1.0);
    // }
    npos_i += dir; // 壁を突破
    npos = npos - npos_i; // 0, 1の範囲に収まっているはず
    // if (length(npos_i) < 0.1) {
      // dbgval=vec4(0.0,1.0,1.0,1.0);  return -1;
    // }
    curpos_i += npos_i;
    curpos_f = npos;
    bool is_inside_aabb = pos3_inside_3(curpos_i, // + curpos_f
      aabb_min * virt3_size, aabb_max * virt3_size);
    if (!is_inside_aabb) {
      break;
    }
  } // for
  if (i == imax) {
    lstr_para = 0.0;
    // hit = i;
      // なんでこの行有効だったのか？ 新バージョンに合わせて無効化しておく。
  }
  if (hit >= 0) {
    if (!hit_tpat) {
      <%if><%is_gl3_or_gles3/>
      value_r = texelFetch(sampler_voxtmax, ivec3(hit_coord) >> tmap_mip, 
	tmap_mip);
      <%else/>
      value_r = <%texture3d/>(sampler_voxtmax, hit_coord / map3_size);
      <%/>
    } else {
      <%if><%is_gl3_or_gles3/>
      value_r = texelFetch(sampler_voxtpax, ivec3(hit_coord) >> tpat_mip,
	tpat_mip);
      <%else/>
      value_r = <%texture3d/>(sampler_voxtpax, (hit_coord) / pattex3_size);
      <%/>
    }
  }
  // if (i > 35) { dbgval = vec4(1.0, 1.0, 0.0, 1.0); }
  // if (hit > 32) { dbgval = vec4(1.0, 0.0, 0.0, 1.0); } // FIXME
  return hit;
}

<%/>
