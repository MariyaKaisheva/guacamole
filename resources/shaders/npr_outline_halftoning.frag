@include "common/header.glsl"

// varyings
in vec2 gua_quad_coords;

@include "common/gua_camera_uniforms.glsl"
@include "common/gua_gbuffer_input.glsl"

uniform int line_thickness;
uniform bool halftoning;
uniform bool apply_outline;
uniform bool no_color;
uniform bool store_for_blending;


// output
//layout(location=0) out vec3 gua_out_color;
//layout(location=2) out vec3 gua_out_normal;
// output
@include "common/gua_fragment_shader_output.glsl"

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
                         0.5, 0.2, 0.9); */ 
 mat4  dither_mat = mat4(1.0, 9.0, 3.0, 11.0,
                          13.0, 5.0, 15.0, 7.0,
                          4.0, 12.0, 2.0, 10.0,
                          16.0, 8.0, 14.0, 6.0) / 17.0; 

  /*float dither_mat [8][8] = {{{1.0/65.0,49.0/65.0, 13.0/65.0, 61.0/65.0, 4.0/65.0, 52.0/65.0, 16.0/65.0, 64.0/65.0}},
                             {33.0/65.0, 17.0/65.0, 45.0/65.0, 29.0/65.0, 36.0/65.0, 20.0/65.0, 48.0/65.0, 32.0/65.0},
                             {{9.0/65.0, 57.0/65.0, 5.0/65.0, 53.0/65.0, 12.0/65.0, 60.0/65.0, 8.0/65.0, 56.0/65.0}},
                             {{41.0/65.0, 25.0/65.0, 37.0/65.0, 21.0/65.0, 44.0/65.0, 28.0/65.0, 40.0/65.0, 24.0/65.0}},
                             {{3.0/65.0, 51.0/65.0, 15.0/65.0, 63.0/65.0, 2.0/65.0, 50.0/65.0, 14.0/65.0, 62.0/65.0}},
                             {{35.0/65.0, 19.0/65.0, 47.0/65.0, 31.0/65.0, 34.0/65.0, 18.0/65.0, 46.0/65.0, 30.0/65.0}},
                             {{11.0/65.0, 59.0/65.0, 7.0/65.0, 55.0/65.0, 10.0/65.0, 58.0/65.0, 6.0/65.0, 54.0/65.0}},
                             {{43.0/65.0, 27/65.0, 39.0/65.0, 23.0/65.0, 42.0/65.0, 26.0/65.0, 38.0/65.0, 22.0/65.0}}};*/
  

  vec3 color = gua_get_color(texcoord).xyz;
  vec3 out_color = vec3(0.0, 0.0, 1.0);
  vec3 accumulated_color_x = vec3(0.0, 0.0, 0.0);
  vec3 accumulated_color_y = vec3(0.0, 0.0, 0.0);


  float cam_to_frag_dist = distance(gua_camera_position_4.xyz/gua_camera_position_4.w, gua_get_position(texcoord) );


//  for (int r = 0; r<1; ++r){
//    for (int c = 0; c<1; ++c){
  for (int r = -1; r<2; ++r){
    for (int c = -1; c<2; ++c){
                   //texcoord.x = (gl_FragCoord.x + r * (line_thickness/cam_to_frag_dist) ) / gua_resolution.x;
                   //texcoord.y = (gl_FragCoord.y + c * (line_thickness/cam_to_frag_dist) ) / gua_resolution.y;
      vec2 tmp_texcoord = vec2( (gl_FragCoord.x + r) / gua_resolution.x, 
                                (gl_FragCoord.y + c) / gua_resolution.y );

      accumulated_color_x += sobel_x[r +1][c +1]* get_linearized_depth(tmp_texcoord);   //for toon shading also color_map look up gives good results
      accumulated_color_y += sobel_y[r +1][c +1]* get_linearized_depth(tmp_texcoord); 
      
      //TODO: outlines from normal map produce artifacts?
      //accumulated_color_x += sobel_x[r +1][c +1] * gua_get_normal(tmp_texcoord);  
      //accumulated_color_y += sobel_y[r +1][c +1] * gua_get_normal(tmp_texcoord);
      
      //accumulated_color_x += sobel_x[r +1][c +1]* gua_get_color(tmp_texcoord); 
      //accumulated_color_y += sobel_y[r +1][c +1]* gua_get_color(tmp_texcoord);
    }
  }

  vec3 edge_color = vec3(sqrt(pow(accumulated_color_x.x, 2) + pow(accumulated_color_y.x, 2)), 
                         sqrt(pow(accumulated_color_x.y, 2) + pow(accumulated_color_y.y, 2)), 
                         sqrt(pow(accumulated_color_x.z, 2) + pow(accumulated_color_y.z, 2)));


  float depth = gua_get_depth();
  float outline_treshhold = 0.03; //TODO meaning of the value?! 
  float color_scale_var = 2.0; //color discritisation value
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

      if(store_for_blending){
        out_color = dithered_color/0.7;
      }
      else{
        out_color = dithered_color;
      }

    }
    else if(apply_outline) { //Outline effect is enabled 

      if(dot(edge_color, vec3(1) ) >= outline_treshhold) {
          out_color = vec3(0.0, 0.0, 0.0); //outline color 
      }
      else{
        if(no_color){
          out_color = vec3(1.0, 1.0, 1.0); //white foreground
        }else{
          out_color = color; //no halftoning; no outline applied
        }
        
      }    
    }
  }
  else {//background color 
    out_color =   vec3(0.1, 0.1, 0.1); //gua_get_color(texcoord); 
  } 

  /*if(depth < 1){ //check for background
    if((halftoning && !apply_outline) || (halftoning && apply_outline && dot(edge_color, vec3(1) ) < outline_treshhold)) { //Halftonning effect is enabled 
      float gray = color.x* 0.2126 + color.y* 0.7152 + color.z* 0.0722;
      vec3 gray_color = vec3(gray, gray, gray);
      int x = int(mod(gl_FragCoord.x, kernel_size));
      int y = int(mod(gl_FragCoord.y, kernel_size)); 
      gray_color = gray_color + gray_color*(dither_mat[x][y]);
      vec3 dithered_color = vec3(0.0, 0.0, 0.0);

      for (int i = 0; i < 8; ++i ){
        for (int j = 0; j < 8; ++j ){
          if (gray_color.x < dither_mat[i][j]) { 
            dithered_color = (floor(gua_get_color(texcoord).xyz*color_scale_var))/color_scale_var;
            //dithered_color = vec3(0.1, 0.1, 0.01);
          }
          else {
            dithered_color = (ceil(gua_get_color(texcoord).xyz*color_scale_var))/color_scale_var;
          }
          gua_out_color = dithered_color;
        }
      }
    }
    else if(apply_outline) { //Outline effect is enabled 

      if(dot(edge_color, vec3(1) ) >= outline_treshhold) {
          gua_out_color = vec3(0.0, 0.0, 0.0); //outline color 
      }
      else{
        if(no_color){
          gua_out_color = vec3(1.0, 1.0, 1.0); //white forground
        }else{
          gua_out_color = gua_get_color(texcoord); //no halftoning; no outline applied
        }
        
      }    
    }
  }
  else {//background color 
    gua_out_color =   vec3(1.0, 1.0, 1.0); //gua_get_color(texcoord); 
  } */

  if(store_for_blending){   
    gua_out_color = color; 
    gua_out_normal = out_color; 

  }
  else{
    gua_out_color = out_color; 
    gua_out_normal = gua_get_normal(texcoord);
  }

  gua_out_pbr = gua_get_pbr(texcoord);
}
