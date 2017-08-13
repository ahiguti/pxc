
uniform float random_seed;

float generate_random(vec3 v)
{
  v.x += fract(random_seed);
  v.y += fract(random_seed);
  return fract(sin(dot(v.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

float fnoise_func(vec2 p)
{
  return generate_random(vec3(floor(p), 0.0));
}

float fnoise3(vec3 p)
{
  vec2 p0 = fract(p.xy * 1024) * 512.0;
  vec2 w = fract(p0);
  float v00 = fnoise_func(p0);
  float v01 = fnoise_func(p0 + vec2(1.0, 0.0));
  float v10 = fnoise_func(p0 + vec2(0.0, 1.0));
  float v11 = fnoise_func(p0 + vec2(1.0, 1.0));
  float v0 = mix(v00 * v00, v01 * v01, w.x);
  float v1 = mix(v10 * v10, v11 * v11, w.x);
  return mix(v0 * v0, v1 * v1, w.y);
}

