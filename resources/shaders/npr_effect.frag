@include "common/header.glsl"

// varyings
in vec2 gua_quad_coords;

@include "common/gua_camera_uniforms.glsl"
@include "common/gua_gbuffer_input.glsl"

uniform int line_thickness;

// output
layout(location=0) out vec3 gua_out_color;



void main() {

  vec2 texcoord = vec2(gl_FragCoord.xy) / gua_resolution.xy;


  //gua_out_color = vec3(gua_get_depth(texcoord));

  // output color
  //gua_out_color = gua_get_color(texcoord);
  //gua_out_color = gua_get_normal(texcoord);


  mat3 sobel_x = mat3 (-1.0, 0.0, 1.0,
                       -2.0, 0.0, 2.0,
                       -1.0, 0.0, 1.0);

  mat3 sobel_y = mat3 (-1.0, -2.0, -1.0,
                        0.0, 0.0, 0.0,
                        1.0, 2.0, 1.0);


  //vec3 color = vec3(gua_get_depth(texcoord));
  vec3 color = gua_get_color(texcoord);
  //vec3 color = gua_get_normal(texcoord);
  vec3 accumulated_color_x = vec3(0.0, 0.0, 0.0);
  vec3 accumulated_color_y = vec3(0.0, 0.0, 0.0);


  float cam_to_frag_dist = distance(gua_camera_position_4.xyz/gua_camera_position_4.w, gua_get_position(texcoord) );


  for (int r = -1; r<2; ++r){
    for (int c = -1; c<2; ++c){
     texcoord.x = (gl_FragCoord.x + r * (line_thickness/cam_to_frag_dist) ) / gua_resolution.x;
     texcoord.y = (gl_FragCoord.y + c * (line_thickness/cam_to_frag_dist) ) / gua_resolution.y;
     // texcoord.x = (gl_FragCoord.x + r) / gua_resolution.x;
      //texcoord.y = (gl_FragCoord.y + c) / gua_resolution.y;

      accumulated_color_x += sobel_x[r +1][c +1]*gua_get_normal(texcoord);
      accumulated_color_y += sobel_y[r +1][c +1]*gua_get_normal(texcoord);

    }
  }

  
  
  

  vec3 edge_color = vec3(sqrt(pow(accumulated_color_x.x, 2) + pow(accumulated_color_y.x, 2)), 
                         sqrt(pow(accumulated_color_x.y, 2) + pow(accumulated_color_y.y, 2)), 
                         sqrt(pow(accumulated_color_x.z, 2) + pow(accumulated_color_y.z, 2)));

  
  if( dot(edge_color, vec3(1) ) < 4.8){
    gua_out_color = gua_get_color(texcoord);
  }
  else { gua_out_color = vec3(0.0, 0.0, 0.0);}
     //discard;

  // output normal
  //gua_out_color = gua_get_normal(texcoord);

  // output position
  //gua_out_color = gua_get_position(texcoord);

 

}

