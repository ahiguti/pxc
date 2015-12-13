
<%if><%eq><%opt/>0<%/>
  <%empty_shader_frag/>
<%else/>
  <%prepend/>
  <%frag_in/> vec3 vary_uvw;
  void main(void)
  {
    /*
    <%if><%enable_normalmapping/>
      vec2 uv0 = vary_uvw.xy / vary_uvw.z;
      vec2 uv_tm = floor(uv0 / tile_size); // tilemap coordinate
      vec2 uv_tmfr = uv0 / tile_size - uv_tm;
	// coordinate inside a tile (0, 1)
      vec2 p = uv_tmfr - 0.5;
      if (dot(p, p) < 0.125) { discard; }
    <%/>
    */
  }
<%/>

