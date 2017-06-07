@include "common/header.glsl"

// varyings
//in vec2 gua_quad_coords;

@include "common/gua_camera_uniforms.glsl"
@include "common/gua_gbuffer_input.glsl"

uniform bool npr_focus;

//sampler
//layout(binding=0) uniform sampler2D blending_texture; 

// output
layout(location=0) out vec3 out_color;
//layout(location=2) out vec3 gua_out_normal;
//@include "common/gua_fragment_shader_output.glsl"

/*float get_linearized_depth(vec2 frag_pos){
  float lin_depth = (2*gua_clip_near)/(gua_clip_far + gua_clip_near - gua_get_unscaled_depth(frag_pos)*(gua_clip_far - gua_clip_near)); 
  lin_depth = lin_depth*(gua_clip_far); //range shift
  return lin_depth;
}*/

void main() {
  vec2 texcoord = vec2(gl_FragCoord.xy) / gua_resolution.xy;
  /*float depth = gua_get_depth();
  vec3 out_color = vec3(0.0);
  vec3 fixed_sphere_position = vec3(10.0, 5.0, 0.0);
  vec3 position = gua_get_position();
  float distance_factor = sphere_radius*10 - sqrt(pow((fixed_sphere_position.x - position.x), 2.0) + pow((fixed_sphere_position.y - position.y), 2.0) + pow((fixed_sphere_position.z - position.z), 2.0));
  //float distance_factor = sphere_radius*10.0 - sqrt(pow((sphere_location_x - position.x), 2.0) + pow((sphere_location_y - position.y), 2.0) + pow((sphere_location_z*5 - position.z), 2.0));
  if(depth < 1){ //foreground color
    vec3 weight_vector = vec3(distance_factor, distance_factor, distance_factor);
    out_color = weight_vector;
    //gua_out_pbr = vec3(0.5, weight_vector.x, 0.5);

  }
  else {//background color 
    out_color = vec3(1.0, 1.0, 1.0);
  } */

  vec3 pbr = gua_get_pbr(texcoord).xyz;
  float weight =  pbr.x;
  if (npr_focus){
   out_color =  gua_get_normal(texcoord)*(1.0 - weight) + gua_get_color(texcoord)*weight; 
  }
  else{
    out_color =  gua_get_normal(texcoord)*weight + gua_get_color(texcoord)*(1.0 - weight); 
  }

  /*if(weight < 0.75)
    out_color = vec3(1.0, 0.0, 0.0);
  else if (weight < 1.0)
    out_color = vec3(0.0, 1.0, 0.0);
  else 
    out_color = vec3(0.0, 0.0, 1.0);*/
}
