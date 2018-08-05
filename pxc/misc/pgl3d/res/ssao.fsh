<%import>pre.fsh<%/>
// FIXME: SSAO

<%decl_fragcolor/>
<%frag_in/> vec2 vary_coord;
uniform vec2 pixel_delta;
uniform sampler2D sampler_tex;
uniform sampler2D sampler_tex_depth;
uniform float option_value;
uniform vec3 near_far_right;

vec3 tex_read(in vec2 delta, in float k)
{
  vec3 v = <%texture2d/>(sampler_tex, vary_coord + delta * pixel_delta).xyz;
  return (v * v) * k;
}

float tex_read_depth(in vec2 delta)
{
  return <%texture2d/>(sampler_tex_depth, vary_coord + delta * pixel_delta).x;

}

const float weight[25] = float[25](
  1., 4., 7., 4., 1.,
  4., 16., 26., 16., 4.,
  7., 26., 41., 26., 7.,
  4., 16., 26., 16., 4.,
  1., 4., 7., 4., 1.
);

float depth_to_z(float depval)
{
  float near = near_far_right.x;
  float far = near_far_right.y;
  return (far * near) / (far - depval * (far - near));
}

void main(void)
{
  // 現状F3で切り替える
  if (option_value == 0.0) {
    vec4 v = <%texture2d/>(sampler_tex, vary_coord);
    <%fragcolor/> = vec4(v.rgb, 1.0);
  } else {
    float p0;
    vec4 dep = <%texture2d/>(sampler_tex_depth, vary_coord);
    float depval = dep.x;
    float near = near_far_right.x;
    float far = near_far_right.y;
    float right = near_far_right.z;
    // float posz = (far * near) / (far - depval * (far - near));
    float posz = depth_to_z(depval);
    vec2 offset = vec2(5.0, 0.0);
    mat2 rot = mat2(-0.1, 1.0, 0.8, 0.0);
    int num_samples = 16;
    float oval = float(num_samples);
    for (int i = 0; i < num_samples; ++i) {
      offset = rot * offset;
      float dist_offset = dot(offset, offset);
      float near_dist = dist_offset * pixel_delta.x * near_far_right.z;
        // nearプレーン上での距離
      float dist = near_dist * posz / near;
        // 距離poszでの距離
      float v0 = depth_to_z(tex_read_depth(offset)) - posz;
      float v1 = depth_to_z(tex_read_depth(-offset)) - posz;
      float z0z1 = v0 + v1;
      if (z0z1 < 0.0) {
        float ang = atan(-z0z1, dist * 2.0); // 0からpi/2まで
        oval = oval - ang * 2.0 / 3.14159;
      }
    }
    oval = oval / float(num_samples);
    oval = mix(1.0, oval, depval * 0.5);
    vec4 v = <%texture2d/>(sampler_tex, vary_coord);
    v.rgb *= oval;
    // v.r = posz;
    // v.rgb = vec3(depval); // FIXME
    <%fragcolor/> = v;
  }
}

