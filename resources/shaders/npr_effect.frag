@include "common/header.glsl"

// varyings
in vec2 gua_quad_coords;

@include "common/gua_camera_uniforms.glsl"
@include "common/gua_gbuffer_input.glsl"

// output
layout(location=0) out vec3 gua_out_color;

void main() {

  vec2 texcoord = vec2(gl_FragCoord.xy) / gua_resolution.xy;


  //gua_out_color = vec3(gua_get_depth(texcoord));

  // output color
  gua_out_color = gua_get_color(texcoord);
        
  // output normal
  gua_out_color = gua_get_normal(texcoord);

  // output position
  gua_out_color = gua_get_position(texcoord);



}

