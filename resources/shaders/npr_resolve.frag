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

///////////////////////////////////////////////////////////////////////////////
vec2
longitude_latitude(in vec3 normal)
{
  const float invpi = 1.0 / 3.14159265359;

  vec2 a_xz = normalize(normal.xz);
  vec2 a_yz = normalize(normal.yz);

  return vec2(0.5 * (1.0 + invpi * atan(a_xz.x, -a_xz.y)),
              acos(-normal.y) * invpi);
}

// https://www.unrealengine.com/blog/physically-based-shading-on-mobile
vec3 EnvBRDFApprox( vec3 SpecularColor, float Roughness, float NoV )
{
  const vec4 c0 = vec4(-1, -0.0275, -0.572, 0.022);
  const vec4 c1 = vec4(1, 0.0425, 1.04, -0.04);
  vec4 r = Roughness * c0 + c1;
  float a004 = min( r.x * r.x, exp2( -9.28 * NoV ) ) * r.x + r.y;
  vec2 AB = vec2( -1.04, 1.04 ) * a004 + r.zw;
  return SpecularColor * AB.x + AB.y;
}

///////////////////////////////////////////////////////////////////////////////
/*vec3 environment_lighting (in ShadingTerms T)
{
  vec3 env_color = vec3(0);
  vec3 brdf_spec = EnvBRDFApprox(T.cspec, T.roughness, dot(T.N, T.V));
  vec3 col1 = vec3(0);
  vec3 col2 = vec3(0);

  switch (gua_environment_lighting_mode) {
    case 0 : // spheremap
      vec2 texcoord = longitude_latitude(T.N);
      col1 = brdf_spec * texture(sampler2D(gua_environment_lighting_texture), texcoord).rgb;
      col2 = brdf_spec * texture(sampler2D(gua_alternative_environment_lighting_texture), texcoord).rgb;
      env_color = mix(col1, col2, gua_environment_lighting_texture_blend_factor);
      break;
    case 1 : // cubemap
      col1 = brdf_spec * texture(samplerCube(gua_environment_lighting_texture), T.N).rgb;
      col2 = brdf_spec * texture(samplerCube(gua_alternative_environment_lighting_texture), T.N).rgb;
      env_color = mix(col1, col2, gua_environment_lighting_texture_blend_factor);
      break;
    case 2 : // single color
      // http://marmosetco.tumblr.com/post/81245981087
      float gua_horizon_fade = 1.3;
      vec3 R = reflect(-T.V, T.N);
      float horizon = saturate( 1.0 + gua_horizon_fade * dot(R, T.N));
      horizon *= horizon;
      vec3 brdf_diff = T.diffuse;
      env_color = (Pi * brdf_diff + (horizon * brdf_spec)) * gua_horizon_fade * gua_environment_lighting_color;
      break;
  };

  return env_color;
}*/

///////////////////////////////////////////////////////////////////////////////
/*vec3 shade_for_all_lights(vec3 color, vec3 normal, vec3 position, vec3 pbr, uint flags, bool ssao_enable) {

  float emit = pbr.r;

  // pass-through check
  if (emit == 1.0) {
    return color;
  }

  ShadingTerms T;
  gua_prepare_shading(T, color, normal, position, pbr);

  vec3 frag_color = vec3(0);
  for (int i = 0; i < gua_lights_num; ++i) {
      float screen_space_shadow = compute_screen_space_shadow (i, position);

      // is it either a visible spot/point light or a sun light ?
      if ( ((bitset[i>>5] & (1u << (i%32))) != 0)
         || i >= gua_lights_num - gua_sun_lights_num )
      {
        frag_color += (1.0 - screen_space_shadow) * gua_shade(i, T);
      }
  }

  float ambient_occlusion = 0.0;
  if (ssao_enable) {
    ambient_occlusion = compute_ssao();
  }
  frag_color += (1.0 - ambient_occlusion) * environment_lighting(T);

  return mix(frag_color, color, emit);
}*/

///////////////////////////////////////////////////////////////////////////////
#if @enable_abuffer@
vec4 abuf_shade(uint pos, float depth) {

  uvec4 data = frag_data[pos];

  vec3 color = vec3(unpackUnorm2x16(data.x), unpackUnorm2x16(data.y).x);
  vec3 normal = vec3(unpackSnorm2x16(data.y).y, unpackSnorm2x16(data.z));
  vec3 pbr = unpackUnorm4x8(data.w).xyz;
  uint flags = bitfieldExtract(data.w, 24, 8);

  vec4 screen_space_pos = vec4(gua_get_quad_coords() * 2.0 - 1.0, depth, 1.0);
  vec4 h = gua_inverse_projection_view_matrix * screen_space_pos;
  vec3 position = h.xyz / h.w;

  vec4 frag_color_emit = vec4(shade_for_all_lights(color, normal, position, pbr, flags, false), pbr.r);
  return frag_color_emit;
}
#endif

///////////////////////////////////////////////////////////////////////////////

// output
layout(location=0) out vec3 gua_out_color;


///////////////////////////////////////////////////////////////////////////////

// skymap

float gua_my_atan2(float a, float b) {
  return 2.0 * atan(a/(sqrt(b*b + a*a) + b));
}

///////////////////////////////////////////////////////////////////////////////
vec3 gua_apply_background_texture() {
  vec3 col1 = texture(sampler2D(gua_background_texture), gua_quad_coords).xyz;
  vec3 col2 = texture(sampler2D(gua_alternative_background_texture), gua_quad_coords).xyz;
  return mix(col1, col2, gua_background_texture_blend_factor);
}

///////////////////////////////////////////////////////////////////////////////
vec3 gua_apply_cubemap_texture() {
  vec3 pos = gua_get_position();
  vec3 view = normalize(pos - gua_camera_position) ;
  vec3 col1 = texture(samplerCube(gua_background_texture), view).xyz;
  vec3 col2 = texture(samplerCube(gua_alternative_background_texture), view).xyz;
  return mix(col1, col2, gua_background_texture_blend_factor);
}

///////////////////////////////////////////////////////////////////////////////
vec3 gua_apply_skymap_texture() {
  vec3 pos = gua_get_position();
  vec3 view = normalize(pos - gua_camera_position);
  const float pi = 3.14159265359;
  float x = 0.5 + 0.5*gua_my_atan2(view.x, -view.z)/pi;
  float y = 1.0 - acos(view.y)/pi;
  vec2 texcoord = vec2(x, y);
  float l = length(normalize(gua_get_position(vec2(0, 0.5)) - gua_camera_position) - normalize(gua_get_position(vec2(1, 0.5)) - gua_camera_position));
  vec2 uv = l*(gua_get_quad_coords() - 1.0)/4.0 + 0.5;
  vec3 col1 = textureGrad(sampler2D(gua_background_texture), texcoord, dFdx(uv), dFdy(uv)).xyz;
  vec3 col2 = textureGrad(sampler2D(gua_alternative_background_texture), texcoord, dFdx(uv), dFdy(uv)).xyz;
  return mix(col1, col2, gua_background_texture_blend_factor);
}

///////////////////////////////////////////////////////////////////////////////
vec3 gua_apply_background_color() {
  return gua_background_color;
}

///////////////////////////////////////////////////////////////////////////////
/*vec3 gua_apply_fog(vec3 color, vec3 fog_color) {
  float dist       = length(gua_camera_position - gua_get_position());
  float fog_factor = clamp((dist - gua_fog_start)/(gua_fog_end - gua_fog_start), 0.0, 1.0);
  return mix(color, fog_color, fog_factor);
}*/

///////////////////////////////////////////////////////////////////////////////
/*vec3 gua_get_background_color() {
  switch (gua_background_mode) {
    case 0: // color
      return sRGB_to_linear(gua_apply_background_color());
    case 1: // skymap texture
      return sRGB_to_linear(gua_apply_skymap_texture());
    case 2: // quad texture
      return sRGB_to_linear(gua_apply_background_texture());
  }
  // cubemap
  return sRGB_to_linear(gua_apply_cubemap_texture());
}*/

///////////////////////////////////////////////////////////////////////////////
/*float get_vignette(float coverage, float softness, float intensity) {
  // inigo quilez's great vigneting effect!
  float a = -coverage/softness;
  float b = 1.0/softness;
  vec2 q = gua_get_quad_coords();
  return clamp(a + b*pow( 16.0*q.x*q.y*(1.0-q.x)*(1.0-q.y), 0.1 ), 0, 1) * intensity + (1-intensity);
} */

///////////////////////////////////////////////////////////////////////////////
vec3 get_point_light_direction(vec3 position){

    LightSource L = gua_lights[0];  //test with single Point light source
    vec3 gua_light_direction = L.position_and_radius.xyz - position;
    float gua_light_distance = length(gua_light_direction);
    gua_light_direction /= gua_light_distance;
    return gua_light_direction;
}

///////////////////////////////////////////////////////////////////////////////  
void main() {

  float depth = gua_get_depth();
  ivec2 frag_pos = ivec2(gl_FragCoord.xy);
  vec2 texcoord = vec2(gl_FragCoord.xy) / gua_resolution.xy;
 
  if(depth < 1 ){
        //gua_out_color = gua_get_normal(texcoord);
        gua_out_color = gua_get_color(texcoord);

        ShadingTerms T;
        gua_prepare_shading(T, gua_get_color(), gua_get_normal(), gua_get_position(), gua_get_pbr()); //out T

        vec3 light_direction_vec = get_point_light_direction(gua_get_position());

      
      //in this case the gua_enable_fog uniform is used as enable_gooch_shading  
      if (gua_enable_fog){
              //Gooch shading model http://www.cs.northwestern.edu/~ago820/SIG98/paper/drawing.html
              float alpha = 0.4;
              float beta = 0.5;
              float b = 0.3; //blue component
              float y = 0.7; //yellow component
              float temp = (1 + clamp(dot(T.N, light_direction_vec), -1.0, 1.0))/2; //  clam not needed here
              vec3 k_cool = vec3(0, 0, b) + alpha*gua_get_color(texcoord);
              vec3 k_warm = vec3(y, y, 0) + beta*gua_get_color(texcoord);
              gua_out_color = temp*k_cool + (1 - temp)*k_warm; 
      }

      else{
        //Cel shading - realtime-rendering 2dn edition
        if (clamp(dot(T.V, T.N), 0.0, 1.0) > 0.25){
              
              float light_test = clamp(dot(T.N, light_direction_vec), -1.0, 1.0);
              if(light_test < 0){
                gua_out_color = T.diffuse;
              }
              else if (light_test < 0.85){
                gua_out_color = T.diffuse*3;
              }
              else {
                gua_out_color = T.diffuse*6;
              }
        } 
        else{
          gua_out_color = vec3(0.1, 0.1, 0.15); //outline color
        }

      }     
        

  }
  else {
    gua_out_color = vec3(0.6, 0.6, 0.6);
  }
}

