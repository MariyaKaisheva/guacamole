@include "common/header.glsl"

///////////////////////////////////////////////////////////////////////////////
// configuration
///////////////////////////////////////////////////////////////////////////////
#define ENABLE_PLOD_FOR_ABUFFER 1

///////////////////////////////////////////////////////////////////////////////
// input
///////////////////////////////////////////////////////////////////////////////
in VertexData {
  vec2 pass_uv_coords;
  vec3 pass_normal;
  vec3 pass_world_position;
  vec3 pass_color;
} VertexIn;

@include "common/gua_fragment_shader_input.glsl"

///////////////////////////////////////////////////////////////////////////////
// output
///////////////////////////////////////////////////////////////////////////////
@include "common/gua_fragment_shader_output.glsl"

///////////////////////////////////////////////////////////////////////////////
// uniforms
///////////////////////////////////////////////////////////////////////////////
@include "common/gua_camera_uniforms.glsl"
@material_uniforms@
uniform uvec2 gua_in_texture; //tmp
layout(binding=0) uniform sampler2D p01_linear_depth_texture;

///////////////////////////////////////////////////////////////////////////////
// functions
///////////////////////////////////////////////////////////////////////////////
@include "common/gua_global_variable_declaration.glsl"
@include "common/gua_abuffer_collect.glsl"

@material_method_declarations_frag@

///////////////////////////////////////////////////////////////////////////////
// main
///////////////////////////////////////////////////////////////////////////////
void main() 
{
  // early discard of fragments not on surfel
  vec2 uv_coords = VertexIn.pass_uv_coords;

  if( dot(uv_coords, uv_coords) > 1) {
    discard;
  }

  @material_input@
  @include "common/gua_global_variable_assignment.glsl"

  vec2 normalized_tex_coords = (uv_coords + 1.0) / 2.0;

  vec4 looked_up_color = texture2D(sampler2D(gua_in_texture), normalized_tex_coords );
  
  //squirel shape
  if( looked_up_color.g > looked_up_color.r && looked_up_color.g > looked_up_color.b 
      && looked_up_color.g - looked_up_color.r >= 20.0/255.0 && looked_up_color.g - looked_up_color.b >= 20.0/255.0)  {
    discard;
  }

  /*if(texture2D(sampler2D(gua_in_texture), normalized_tex_coords ).a < 0.5){
    discard;
  }*/

  gua_color = vec3(texture2D(sampler2D(gua_in_texture), normalized_tex_coords).xyz);
  //gua_color      = pow(VertexIn.pass_color, vec3(1.4));
  gua_normal     = VertexIn.pass_normal;
  gua_metalness  = 0.0;
  gua_roughness  = 1.0;
  gua_emissivity = 1.0; // pass through if unshaded

  vec3 gua_world_position = VertexIn.pass_world_position;

  // normal mode or high fidelity shadows
  if (gua_rendering_mode != 1) {
    @material_method_calls_frag@
  }

  // clipping against clipping planes is performed in submit_fragment
  submit_fragment(gl_FragCoord.z);
}

