@include "common/header.glsl"

// varyings
in vec2 gua_quad_coords;

@include "common/gua_camera_uniforms.glsl"
@include "common/gua_gbuffer_input.glsl"

uniform int line_thickness;
uniform bool halftoning;
uniform bool apply_outline;

// output
layout(location=0) out vec3 gua_out_color;



void main() {

  vec2 texcoord = vec2(gl_FragCoord.xy) / gua_resolution.xy;

  mat3 sobel_x = mat3 (-1.0, 0.0, 1.0,
                       -2.0, 0.0, 2.0,
                       -1.0, 0.0, 1.0);

  mat3 sobel_y = mat3 (-1.0, -2.0, -1.0,
                        0.0, 0.0, 0.0,
                        1.0, 2.0, 1.0);
            
  mat3 dither_mat = mat3(0.1, 0.8, 0.4,
                         0.7, 0.6, 0.3,
                         0.5, 0.2, 0.9);             


  //vec3 color = vec3(gua_get_depth(texcoord));
  vec3 color = gua_get_color(texcoord);
  //vec3 color = gua_get_normal(texcoord);
  vec3 accumulated_color_x = vec3(0.0, 0.0, 0.0);
  vec3 accumulated_color_y = vec3(0.0, 0.0, 0.0);


  float cam_to_frag_dist = distance(gua_camera_position_4.xyz/gua_camera_position_4.w, gua_get_position(texcoord) );


  for (int r = -1; r<2; ++r){
    for (int c = -1; c<2; ++c){
     //texcoord.x = (gl_FragCoord.x + r * (line_thickness/cam_to_frag_dist) ) / gua_resolution.x;
     //texcoord.y = (gl_FragCoord.y + c * (line_thickness/cam_to_frag_dist) ) / gua_resolution.y;
      vec2 tmp_texcoord = vec2( (gl_FragCoord.x + r) / gua_resolution.x, 
                                (gl_FragCoord.y + c) / gua_resolution.y );

      accumulated_color_x += sobel_x[r +1][c +1]*gua_get_normal(tmp_texcoord); //for toon shading also color_map look up gives good results
      accumulated_color_y += sobel_y[r +1][c +1]*gua_get_normal(tmp_texcoord);

    }
  }
  

  vec3 edge_color = vec3(sqrt(pow(accumulated_color_x.x, 2) + pow(accumulated_color_y.x, 2)), 
                         sqrt(pow(accumulated_color_x.y, 2) + pow(accumulated_color_y.y, 2)), 
                         sqrt(pow(accumulated_color_x.z, 2) + pow(accumulated_color_y.z, 2)));


  float depth = gua_get_depth(); 
  
  if(depth < 1)//check for background
  { 

    if(halftoning && dot(edge_color, vec3(1) ) < 2.8){

     // if(dot(edge_color, vec3(1) ) < 2.8){
          float gray = (gua_get_color(texcoord).r + gua_get_color(texcoord).g +gua_get_color(texcoord).b)/3;
          vec3 gray_color = vec3(gray, gray, gray);
          int x = int(mod(gl_FragCoord.x, 3));
          int y = int(mod(gl_FragCoord.y, 3)); 
          gray_color = gray_color + gray_color*(dither_mat[x][y]);
      
          if (gray_color.x < dither_mat[x][y]){ 
              gray_color = (floor(gua_get_color(texcoord).xyz*4))/4.0;
          }
          else {
              gray_color = (ceil(gua_get_color(texcoord).xyz*4))/4.0;
          }
          gua_out_color = gray_color;
      }
    else{
      if(apply_outline && dot(edge_color, vec3(1) ) >= 2.8){
        gua_out_color = vec3(0.18, 0.18, 0.18); //outline color 
      }
      else{
        gua_out_color = gua_get_color(texcoord); //no halftoning; no outline applied
      }  
        
    } 
       
  } 
else { gua_out_color = vec3(0.25, 0.2, 0.3);} //background color 


}
