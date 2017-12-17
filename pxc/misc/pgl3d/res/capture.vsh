<%import>pre.vsh<%/>

<%vert_in/> vec2 vert;
<%vert_out/> vec2 vary_coord;
void main(void)
{
  vary_coord = (vert + 1.0) * 0.5;
  gl_Position = vec4(vert, 0.0, 1.0);
}

