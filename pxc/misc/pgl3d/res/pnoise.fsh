
// const float pnoise_mod = 256.0;
const float pnoise_mod = 16.0;

/*
const float pnoise_pt[] = float[](
  3,5,11,7,9,8,4,2,6,0,8,13,4,11,3,1);
*/
const float pnoise_pt[] = float[](
  147,101,155,103,249,104,116,226,54,208,184,205,52,11,195,241,84,49,
  33,25,99,169,140,93,160,158,162,197,215,24,215,63,88,81,130,94,20,
  45,75,147,227,56,116,112,151,197,187,150,254,68,108,95,150,207,61,
  59,102,106,150,32,73,111,13,155,38,240,154,111,156,121,143,238,187,
  246,96,26,70,160,227,103,178,71,25,63,137,15,232,80,184,156,96,74,
  37,17,196,229,68,137,117,158,239,68,74,59,207,184,203,249,180,91,2,
  44,2,139,59,38,185,212,119,119,70,6,81,227,142,236,219,35,128,189,
  245,216,243,108,70,105,106,233,99,207,48,238,6,230,107,75,133,51,
  109,57,82,115,77,221,145,208,53,113,126,135,147,14,15,128,195,232,
  168,202,68,119,174,33,207,92,255,253,66,13,145,175,136,156,51,213,
  121,187,233,65,248,189,117,134,88,52,47,165,85,49,241,85,94,9,250,
  170,74,70,106,111,34,112,212,84,6,169,35,225,63,249,117,224,253,31,
  147,166,54,32,122,108,176,171,106,177,75,134,154,93,65,99,46,127,88,
  88,231,5,77,222,225,143,30,139,178,172,229,4,121,63
);
/*
*/

const vec3 pnoise_gt[16] = vec3[16](
  vec3(1,1,0),vec3(-1,1,0),vec3(1,-1,0),vec3(-1,-1,0),
  vec3(1,0,1),vec3(-1,0,1),vec3(1,0,-1),vec3(-1,0,-1),
  vec3(0,1,1),vec3(0,-1,1),vec3(0,1,-1),vec3(0,-1,-1),
  vec3(1,1,0),vec3(0,-1,1),vec3(-1,1,0),vec3(0,-1,-1)
);

float pnoise_gpt(float i)
{
  /*
  uint j = uint(fract(i * (1.0/16.0)) * 16.0);
  float r = 0.0;
  switch (j) {
  case 0u: r = 3.0; break;
  case 1u: r = 5.0; break;
  case 2u: r = 11.0; break;
  case 3u: r = 7.0; break;
  case 4u: r = 9.0; break;
  case 5u: r = 8.0; break;
  case 6u: r = 4.0; break;
  case 7u: r = 2.0; break;
  case 8u: r = 6.0; break;
  case 9u: r = 0.0; break;
  case 10u: r = 13.0; break;
  case 11u: r = 4.0; break;
  case 12u: r = 11.0; break;
  case 13u: r = 3.0; break;
  case 14u: r = 1.0; break;
  case 15u: r = 16.0; break;
  }
  return r;
  */
  /*
  i = fract(i * (1.0/16.0)) * 16.0;
  if (i >= 15.0) { return 1.0; }
  if (i >= 14.0) { return 3.0; }
  if (i >= 13.0) { return 11.0; }
  if (i >= 12.0) { return 4.0; }
  if (i >= 11.0) { return 13.0; }
  if (i >= 10.0) { return 8.0; }
  if (i >=  9.0) { return 0.0; }
  if (i >=  8.0) { return 6.0; }
  if (i >=  7.0) { return 2.0; }
  if (i >=  6.0) { return 4.0; }
  if (i >=  5.0) { return 8.0; }
  if (i >=  4.0) { return 9.0; }
  if (i >=  3.0) { return 7.0; }
  if (i >=  2.0) { return 11.0; }
  if (i >=  1.0) { return 5.0; }
  return 3.0;
  */
  return pnoise_pt[uint(i) & (uint(pnoise_mod) - 1u)];
  /*
  */
  // return pnoise_pt[i];
}

float pnoise_fade(float v)
{
  // return v;
  // return v * v * v * (v * (v * 6.0 - 15.0) + 10.0);
  return smoothstep(0.0, 1.0, v);
}

float pnoise_grad(float h8, float x, float y, float z)
{
  uint h = uint(h8) & 15u;
  /*
  return dot(pnoise_gt[h], vec3(x, y, z));
  */
  float u = h < 8u ? x : y;
  float v = h < 4u ? y : (h == 12u || h == 14u) ? x : z;
  return ((h & 1u) == 0u ? u : -u) + ((h & 2u) == 0u ? v : -v);
  /*
  */
}

float pnoise3(vec3 p)
{
  vec3 i = floor(fract(p * (1.0 / pnoise_mod)) * pnoise_mod);
  vec3 pf = fract(p);
  vec3 fd = vec3(pnoise_fade(pf.x), pnoise_fade(pf.y), pnoise_fade(pf.z));
  float ia = pnoise_gpt(i.x) + i.y;
  float iaa = pnoise_gpt(ia) + i.z;
  float iab = pnoise_gpt(ia+1u) + i.z;
  float ib = pnoise_gpt(i.x+1u) + i.y;
  float iba = pnoise_gpt(ib) + i.z;
  float ibb = pnoise_gpt(ib+1u) + i.z;
  float t0 = pnoise_grad(pnoise_gpt(iaa), pf.x, pf.y, pf.z);
  float t1 = pnoise_grad(pnoise_gpt(iba), pf.x-1.0, pf.y, pf.z);
  float t2 = pnoise_grad(pnoise_gpt(iab), pf.x, pf.y-1.0, pf.z);
  float t3 = pnoise_grad(pnoise_gpt(ibb), pf.x-1.0, pf.y-1.0, pf.z);
  float t4 = pnoise_grad(pnoise_gpt(iaa+1u), pf.x, pf.y, pf.z-1.0);
  float t5 = pnoise_grad(pnoise_gpt(iba+1u), pf.x-1.0, pf.y, pf.z-1.0);
  float t6 = pnoise_grad(pnoise_gpt(iab+1u), pf.x, pf.y-1.0, pf.z-1.0);
  float t7 = pnoise_grad(pnoise_gpt(ibb+1u), pf.x-1.0, pf.y-1.0, pf.z-1.0);
  return mix(mix(mix(t0, t1, fd.x), mix(t2, t3, fd.x), fd.y),
	     mix(mix(t4, t5, fd.x), mix(t6, t7, fd.x), fd.y), fd.z);
}

