@include "common/header.glsl"

// varying input
in vec2 gua_quad_coords;

// uniforms
@include "common/gua_camera_uniforms.glsl"
@include "common/gua_resolve_pass_uniforms.glsl"

// gbuffer input
@include "common/gua_gbuffer_input.glsl"

// methods
@include "common/gua_shading.glsl"
@include "common/gua_tone_mapping.glsl"

@include "ssao.frag"
@include "screen_space_shadow.frag"

#define ABUF_MODE readonly
#define ABUF_SHADE_FUNC abuf_shade
@include "common/gua_abuffer_resolve.glsl"

uint bitset[((@max_lights_num@ - 1) >> 5) + 1];


//uniform int factor; 
///////////////////////////////////////////////////////////////////////////////

// output
layout(location=0) out vec3 gua_out_color;
layout(location=2) out vec3 gua_out_normal; 

///////////////////////////////////////////////////////////////////////////////

float gua_my_atan2(float a, float b) {
  return 2.0 * atan(a/(sqrt(b*b + a*a) + b));
}


///////////////////////////////////////////////////////////////////////////////
vec3 get_point_light_direction(vec3 position){

    LightSource L = gua_lights[0];  //test with single Point light source
    vec3 gua_light_direction = L.position_and_radius.xyz - position;
    float gua_light_distance = length(gua_light_direction);
    gua_light_direction /= gua_light_distance;
    return gua_light_direction;
}

///////////////////////////////////////////////////////////////////////////////

float get_weigt(vec2 neighbour_coord, vec2 target_coord){
  float sigma_d = 0.5;  
  float spatial_distance_term =  pow((target_coord.x - neighbour_coord.x), 2) + pow((target_coord.y - neighbour_coord.y), 2);
  spatial_distance_term /= 2.0*sigma_d*sigma_d;
  float weight =  (exp(-spatial_distance_term))/(2*3.14*sigma_d*sigma_d);
  return weight;
}

///////////////////////////////////////////////////////////////////////////////
vec3 get_averaged_normal(vec2 texcoord){
      vec3 new_normal = vec3(0.0, 0.0, 0.0);
      float summed_weight = 0.0;
      int kernel_size = 2;

      for (int r =  - kernel_size  ; r <=  kernel_size  ; ++r){
        for(int c =  - kernel_size ; c <=  kernel_size ; ++c){
          float x_coord = (gl_FragCoord.x + r) / gua_resolution.x;
          float y_coord = (gl_FragCoord.y + c) / gua_resolution.y;
          vec2 current_neighbour_texcoord = vec2(x_coord, y_coord);
          float weight = get_weigt(current_neighbour_texcoord, texcoord);
          new_normal += gua_get_normal(current_neighbour_texcoord)*weight;
          summed_weight += weight;
        }
      }
      gua_out_normal = new_normal.xyz/summed_weight;
      return new_normal.xyz/summed_weight; 
}
///////////////////////////////////////////////////////////////////////////////


void main() {

  float depth = gua_get_depth();
  ivec2 frag_pos = ivec2(gl_FragCoord.xy);
  vec2 texcoord = vec2(gl_FragCoord.xy) / gua_resolution.xy;
  int factor = 7;
 
  if(depth < 1 ){
        //gua_out_color = gua_get_normal(texcoord);
        gua_out_color = gua_get_color(texcoord);
       // vec3 smooth_normal = get_averaged_normal(texcoord);

        ShadingTerms T;
        gua_prepare_shading(T, gua_get_color(), gua_get_normal()/* smooth_normal*/, gua_get_position(), gua_get_pbr()); //out T

        vec3 light_direction_vec = get_point_light_direction(gua_get_position());

      
      
      if (gua_enable_gooch_shading){
              //Gooch shading model http://www.cs.northwestern.edu/~ago820/SIG98/paper/drawing.html
              float alpha = 0.4;
              float beta = 0.8;
              float b = 0.3; //blue component
              float y = 0.4; //yellow component
              float temp = (1 + clamp(dot(T.N, light_direction_vec), -1.0, 1.0))/2; //  clam not needed here
              vec3 k_cool = vec3(0, 0, b)  + alpha*gua_get_color(texcoord);
              vec3 k_warm = vec3(y, y, 0) + beta*gua_get_color(texcoord);
              gua_out_color = (1 - temp)*k_cool + temp*k_warm; 
             // gua_out_color = temp*k_cool + (1 - temp)*k_warm;  //swap  'temp' and '1-temp' terms to inverse light source impression
                                                                //fromula difference: http://developer.amd.com/wordpress/media/2012/10/ShaderX_NPR.pdf
      }

      else{
        //Cel shading - realtime-rendering 2dn edition
        if (clamp(dot(T.V, T.N), 0.0, 1.0) > 0.25){
              
              float light_test = clamp(dot(T.N, light_direction_vec), -1.0, 1.0);
              if(light_test < 0){
                gua_out_color = T.diffuse;
              }
              else if (light_test < 0.85){
                gua_out_color = T.diffuse*factor;
              }
              else {
                gua_out_color =  T.diffuse*factor*2;
              }
        } 
        else{
           //outline color
          //gua_out_color = vec3(1.0, 0.0, 0.0);
         gua_out_color = T.diffuse;
        }

      }     
  }
  else {
    gua_out_color = vec3(0.6, 0.6, 0.6);
  }
}

