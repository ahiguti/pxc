
<%prepend/>
<%if><%light_fixed/>
  uniform vec3 trans;
  uniform float scale;
<%>
  uniform mat4 shadowmap_vp;
<%/>
<%decl_instance_attr mat4 model_matrix/>;
<%vert_in/> vec3 position;
<%if><%not><%eq><%opt/>0<%/><%/>
  <%vert_in/> vec3 uvw;
  <%vert_out/> vec3 vary_uvw;
<%/>
<%if><%not><%enable_depth_texture/><%/>
  <%vert_out/> vec4 vary_smpos;
<%/>
void main(void)
{
  <%if><%light_fixed/>
    vec4 p = <%instance_attr model_matrix/> * vec4(position, 1.0);
    vec3 p1 = p.xyz + trans;
    p1 *= scale
    p = vec4(p1, 1.0);
    gl_Position = p;
  <%>
    vec4 p = shadowmap_vp * (<%instance_attr model_matrix/>
      * vec4(position, 1.0));
    gl_Position = p;
  <%/>
  <%if><%not><%enable_depth_texture/><%/>
    vary_smpos = p;
  <%/>
  <%if><%not><%eq><%opt/>0<%/><%/>
    vary_uvw = uvw;
  <%/>
}

  string f;
  if (g.enable_depth_texture && opt == 0) {
    f += g.empty_shader_frag();
  } else {
    f += g.prepend();
    if (opt != 0) {
      f += g.frag_in() + "vec3 vary_uvw;\n";
    }
    if (!g.enable_depth_texture) {
      f += g.frag_in() + "vec4 vary_smpos;\n";
      f += g.decl_fragcolor();
    }
    f += "void main(void) {\n";
    /*
    if (opt != 0) {
      if (g.enable_normalmapping) {
	f += "vec2 uv0 = vary_uvw.xy / vary_uvw.z;\n";
	f += "vec2 uv_tm = floor(uv0 / tile_size);\n"; // tilemap coordinate
	f += "vec2 uv_tmfr = uv0 / tile_size - uv_tm;\n";
	  // coordinate inside a tile (0, 1)
	f += "vec2 uvp = uv_tmfr - 0.5;\n";
	f += "if (dot(uvp, uvp) < 0.125) { discard; }\n";
      }
    }
    */
    if (g.enable_vsm) {
      f += "  vec4 p = vary_smpos;\n";
      f += "  float pz = (p.z/p.w + 1.0) / 2.0;\n"; // TODO: calc in vsh
      f += g.fragcolor() + "= vec4(pz, pz * pz, 0.0, 0.0);\n";
    } else if (!g.enable_depth_texture) {
      f += "  vec4 p = vary_smpos;\n";
      f += "  float pz = (p.z/p.w + 1.0) / 2.0;\n"; // TODO: calc in vsh
      // TODO: vectorize
      f += "  float z = pz * 256.0;\n"; // [0.0, 256.0]
      f += "  float z0 = floor(z);\n"; // [0, 256]
      f += "  z = (z - z0) * 256.0;\n"; // [0.0, 256.0)
      f += "  float z1 = floor(z);\n"; // [0, 256)
      f += "  z = (z - z1) * 256.0;\n"; // [0.0, 256.0)
      f += "  float z2 = floor(z);\n";  // [0, 256)
      f += g.fragcolor() + "= vec4(z0/255.0, z1/255.0, z2/255.0, 1.0);\n";
    }
    f += "}\n";
  }
  return make_glshader_ptr{
    shadowmap_uniforms,
    triangles_instance_attributes,
    shadowmap_vertex_attributes
  }(v, f, "model_matrix", g.debug_level);
}

