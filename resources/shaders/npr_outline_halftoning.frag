@include "common/header.glsl"

// varyings
in vec2 gua_quad_coords;

@include "common/gua_camera_uniforms.glsl"
@include "common/gua_gbuffer_input.glsl"

uniform int line_thickness;
uniform bool halftoning;
uniform bool apply_outline;
uniform bool no_color;

// output
layout(location=0) out vec3 gua_out_color;

float get_linearized_depth(vec2 frag_pos){
  float lin_depth = (2*gua_clip_near)/(gua_clip_far + gua_clip_near - gua_get_unscaled_depth(frag_pos)*(gua_clip_far - gua_clip_near)); 
  lin_depth = lin_depth*(gua_clip_far); //range shift
  return lin_depth;
}

void main() {

  vec2 texcoord = vec2(gl_FragCoord.xy) / gua_resolution.xy;

  mat3 sobel_x = mat3 (-1.0, 0.0, 1.0,
                       -2.0, 0.0, 2.0,
                       -1.0, 0.0, 1.0);

  mat3 sobel_y = mat3 (-1.0, -2.0, -1.0,
                        0.0, 0.0, 0.0,
                        1.0, 2.0, 1.0);
            
  /*mat3 dither_mat = mat3(0.1, 0.8, 0.4,
                         0.7, 0.6, 0.3,
                         0.5, 0.2, 0.9);  */
 mat4  dither_mat = mat4(1.0, 9.0, 3.0, 11.0,
                          13.0, 5.0, 15.0, 7.0,
                          4.0, 12.0, 2.0, 10.0,
                          16.0, 8.0, 14.0, 6.0) / 17; 

  /*float dither_mat [8][8] = {{{1/65,49/65, 13/65, 61/65, 4/65, 52/65, 16/65, 64/65}},
                             {33/65, 17/65, 45/65, 29/65, 36/65, 20/65, 48/65, 32/65},
                             {{9/65, 57/65, 5/65, 53/65, 12/65, 60/65, 8/65, 56/65}},
                             {{41/65, 25/65, 37/65, 21/65, 44/65, 28/65, 40/65, 24/65}},
                             {{3/65, 51/65, 15/65, 63/65, 2/65, 50/65, 14/65, 62/65}},
                             {{35/65, 19/65, 47/65, 31/65, 34/65, 18/65, 46/65, 30/65}},
                             {{11/65, 59/65, 7/65, 55/65, 10/65, 58/65, 6/65, 54/65}},
                             {{43/65, 27/65, 39/65, 23/65, 42/65, 26/65, 38/65, 22/65}}};*/
  

  vec3 color = gua_get_color(texcoord);
  vec3 accumulated_color_x = vec3(0.0, 0.0, 0.0);
  vec3 accumulated_color_y = vec3(0.0, 0.0, 0.0);


  float cam_to_frag_dist = distance(gua_camera_position_4.xyz/gua_camera_position_4.w, gua_get_position(texcoord) );


  for (int r = -1; r<2; ++r){
    for (int c = -1; c<2; ++c){
                   //texcoord.x = (gl_FragCoord.x + r * (line_thickness/cam_to_frag_dist) ) / gua_resolution.x;
                   //texcoord.y = (gl_FragCoord.y + c * (line_thickness/cam_to_frag_dist) ) / gua_resolution.y;
      vec2 tmp_texcoord = vec2( (gl_FragCoord.x + r) / gua_resolution.x, 
                                (gl_FragCoord.y + c) / gua_resolution.y );

      //accumulated_color_x += sobel_x[r +1][c +1]* get_linearized_depth(tmp_texcoord);   //for toon shading also color_map look up gives good results
      //accumulated_color_y += sobel_y[r +1][c +1]* get_linearized_depth(tmp_texcoord); 
      
      accumulated_color_x += sobel_x[r +1][c +1]* gua_get_normal(tmp_texcoord); 
                                                                                accumulated_color_y += sobel_y[r +1][c +1]* gua_get_normal(tmp_texcoord);
      
      //accumulated_color_x += sobel_x[r +1][c +1]* gua_get_color(tmp_texcoord); 
      //accumulated_color_y += sobel_y[r +1][c +1]* gua_get_color(tmp_texcoord);
    }
  }

  vec3 edge_color = vec3(sqrt(pow(accumulated_color_x.x, 2) + pow(accumulated_color_y.x, 2)), 
                         sqrt(pow(accumulated_color_x.y, 2) + pow(accumulated_color_y.y, 2)), 
                         sqrt(pow(accumulated_color_x.z, 2) + pow(accumulated_color_y.z, 2)));


  float depth = gua_get_depth();
  float outline_treshhold = 2.3; //TODO meaning of the value?! 
  float color_scale_var = 8.0; //color discritisation value
  int kernel_size = 4; //changes the size repeated pattern grid; influences the showerdoor effect; should correspond to dither_mat dimensions

  if(depth < 1){ //check for background
    if((halftoning && !apply_outline) || (halftoning && apply_outline && dot(edge_color, vec3(1) ) < outline_treshhold)) { //Halftonning effect is enabled 
      float gray = color.x* 0.2126 + color.y* 0.7152 + color.z* 0.0722;
      vec3 gray_color = vec3(gray, gray, gray);
      int x = int(mod(gl_FragCoord.x, kernel_size));
      int y = int(mod(gl_FragCoord.y, kernel_size)); 
      gray_color = gray_color + gray_color*(dither_mat[x][y]);
      vec3 dithered_color = vec3(0.0, 0.0, 0.0);
      if (gray_color.x < dither_mat[x][y]) { 
       dithered_color = (floor(gua_get_color(texcoord).xyz*color_scale_var))/color_scale_var;
      }
      else {
        dithered_color = (ceil(gua_get_color(texcoord).xyz*color_scale_var))/color_scale_var;
      }
      gua_out_color = dithered_color;
    }
    else if(apply_outline) { //Outline effect is enabled 

      if(dot(edge_color, vec3(1) ) >= outline_treshhold) {
          gua_out_color = vec3(0.0, 0.0, 0.0); //outline color 
      }
      else{
        if(no_color){
          gua_out_color = vec3(1.0, 1.0, 1.0);
        }else{
          gua_out_color = gua_get_color(texcoord); //no halftoning; no outline applied
        }
        
      }    
    }
}
else {//background color 
 gua_out_color =  gua_get_color(texcoord); 
} 

}
