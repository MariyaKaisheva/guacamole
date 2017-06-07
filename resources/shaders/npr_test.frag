@include "common/header.glsl"

// varyings
in vec2 gua_quad_coords;

@include "common/gua_camera_uniforms.glsl"
@include "common/gua_gbuffer_input.glsl"

uniform float sphere_radius;
uniform vec3 sphere_location;


// output
@include "common/gua_fragment_shader_output.glsl"

void main() {
  vec2 texcoord = vec2(gl_FragCoord.xy) / gua_resolution.xy;
  float depth = gua_get_depth();
  vec3 out_weight = vec3(0.0);
  vec3 fixed_sphere_position = vec3(10.0, 5.0, 0.0);
  vec3 position = gua_get_position();
  //float distance_factor = sphere_radius*10.0 - sqrt(pow((fixed_sphere_position.x - position.x), 2.0) + pow((fixed_sphere_position.y - position.y), 2.0) + pow((fixed_sphere_position.z - position.z), 2.0));
  //float distance_factor = sphere_radius*5.0 - sqrt(pow((sphere_location.x - position.x), 2.0) + pow((sphere_location.y - position.y), 2.0) + pow((sphere_location.z*20.0 - position.z), 2.0));
  float distance_factor = distance(sphere_location, position)*10.0 - sphere_radius;
  if(depth < 1){ //foreground color
    vec3 weight_vector = vec3(distance_factor);
    out_weight = clamp(weight_vector, 0.0, 1.0);
  }
  else {//background color 
    out_weight = vec3(1.0, 1.0, 1.0);
  } 
  gua_out_color = gua_get_color();
  gua_out_normal = gua_get_normal();
 // gua_out_pbr = clamp(out_weight, 0.0, 1.0); 

  //http://stackoverflow.com/questions/5731863/mapping-a-numeric-range-onto-another
  gua_out_pbr = vec3(1.0/sphere_radius)*(min(distance(sphere_location, position), sphere_radius));
}
