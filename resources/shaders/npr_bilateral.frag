@include "common/header.glsl"

// varyings
in vec2 gua_quad_coords;

@include "common/gua_camera_uniforms.glsl"
@include "common/gua_gbuffer_input.glsl"

uniform int line_thickness;
//uniform bool halftoning;

//smoothing parameters
//uniform float sigma_d;
//uniform float sigma_r;

// output
layout(location=0) out vec3 gua_out_color;

//smoothing parameters
float sigma_d = 0.5;
float sigma_r = 0.5; 

float get_intensity(vec2 texcoord){
  vec3 color = gua_get_color(texcoord);
  float gray_color_intensity = color.x* 0.2126 + color.y* 0.7152 + color.z* 0.0722; //should be substituted with CEI_lab distance for color images 
  return gray_color_intensity;
}


float compute_weight(vec2 neighbout_coord, vec2 target_coord){
  
  float spatial_distance_term =  pow((target_coord.x - neighbout_coord.x), 2) + pow((target_coord.y - neighbout_coord.y), 2);
  float range_distance_term =   pow((abs(get_intensity(target_coord) - get_intensity(neighbout_coord))), 2);
  spatial_distance_term /= 2.0*sigma_d*sigma_d;
  range_distance_term /= 2.0*sigma_r*sigma_r;
  float weight = exp(-spatial_distance_term- range_distance_term);
  return weight;
}

float compute_gaussian_weight(vec2 neighbout_coord, vec2 target_coord){
  
  float spatial_distance_term =  pow((target_coord.x - neighbout_coord.x), 2) + pow((target_coord.y - neighbout_coord.y), 2);
 // float range_distance_term =   pow((abs(get_intensity(target_coord) - get_intensity(neighbout_coord))), 2);
  spatial_distance_term /= 2.0*sigma_d*sigma_d;
 // range_distance_term /= 2.0*sigma_r*sigma_r;
 
  float weight =  (exp(-spatial_distance_term))/(2*3.14*sigma_d*sigma_d);
  return weight;
}

void main() {

  vec2 texcoord = vec2(gl_FragCoord.xy ) / gua_resolution.xy;

  vec3 color = vec3(0.0, 0.0, 0.0);// = gua_get_color(texcoord);
  //vec3 color = gua_get_normal(texcoord);
  vec3 accumulated_color_x = vec3(0.0, 0.0, 0.0);
  vec3 accumulated_color_y = vec3(0.0, 0.0, 0.0);


  //float cam_to_frag_dist = distance(gua_camera_position_4.xyz/gua_camera_position_4.w, gua_get_position(texcoord) );
  int kernel_size = line_thickness; //gives offset to target pixel 
  float sum_weight = 0.0;


  for (int r = - kernel_size ; r <= kernel_size; ++r){
    for (int c = - kernel_size; c <= kernel_size; ++c){


     float x_coord = (gl_FragCoord.x + r) / gua_resolution.x;
     float y_coord = (gl_FragCoord.y + c) / gua_resolution.y;     
     vec2 current_texcoord =  vec2(x_coord, y_coord); 
     float current_weight = compute_gaussian_weight(current_texcoord, texcoord); 
     color += gua_get_color(current_texcoord)*current_weight; 
     sum_weight += current_weight;
    }
  }
  
  color = color/sum_weight;

  float depth = gua_get_depth(); 
  if(depth < 1){
     gua_out_color = color;
     //gua_out_color = color + vec3(1.2, 0.2, 0.2);
  }    
  else {
    gua_out_color = vec3(0.2, 0.2, 0.2); //background color 
  } 
  
}
