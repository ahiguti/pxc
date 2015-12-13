
<%if><%and><%enable_depth_texture/><%eq><%opt/>0<%/><%/>
  <%empty_shader_frag/>
<%else/>
  <%prepend/>
  <%if><%not><%eq><%opt/>0<%/><%/>
    <%frag_in/> vec3 vary_uvw;
  <%/>
  <%if><%not><%enable_depth_texture/><%/>
    <%frag_in/> vec4 vary_smpos;
    <%decl_fragcolor/>
  <%/>
  void main(void)
  {
    /*
    <%if><%not><%eq><%opt/>0<%/><%/>
      <%if><%enable_normalmapping/>
	vec2 uv0 = vary_uvw.xy / vary_uvw.z;
	vec2 uv_tm = floor(uv0 / tile_size); // tilemap coordinate
	vec2 uv_tmfr = uv0 / tile_size - uv_tm;
	  // coordinate inside a tile (0, 1)
	vec2 uvp = uv_tmfr - 0.5;
	if (dot(uvp, uvp) < 0.125) { discard; }
      <%/>
    <%/>
    */
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
  }
<%/>
