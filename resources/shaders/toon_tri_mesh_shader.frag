@include "common/header.glsl"

//layout (early_fragment_tests) in;

//@include "common/gua_fragment_shader_input.glsl"

// input from geometry shader
in  VertexData{
 //output for fragment shader
 vec3  gua_varying_world_position;
 vec3  gua_varying_view_position;
 vec3  gua_varying_normal;
 vec3  gua_varying_tangent;
 vec3  gua_varying_bitangent;
 vec2  gua_varying_texcoords;
 vec3  gua_varying_color;
 float gua_varying_roughness;
 float gua_varying_metalness;
 float gua_varying_emissivity;
} GeometryIn;

@include "common/gua_camera_uniforms.glsl"

uniform float gua_texel_width;
uniform float gua_texel_height;

vec2 gua_get_quad_coords() {
  return vec2(gl_FragCoord.x * gua_texel_width, gl_FragCoord.y * gua_texel_height);
}

@material_uniforms@

@include "common/gua_fragment_shader_output.glsl"
@include "common/gua_global_variable_declaration.glsl"

@include "common/gua_abuffer_collect.glsl"

@material_method_declarations_frag@

void main() {

  @material_input@
  //@include "common/gua_global_variable_assignment.glsl"
  gua_world_position    = GeometryIn.gua_varying_world_position;
  gua_normal            = normalize(GeometryIn.gua_varying_normal);
  gua_tangent           = normalize(GeometryIn.gua_varying_tangent);
  gua_bitangent         = normalize(GeometryIn.gua_varying_bitangent);
  gua_texcoords         = GeometryIn.gua_varying_texcoords;
  gua_color             = GeometryIn.gua_varying_color;
  gua_roughness         = GeometryIn.gua_varying_roughness;
  gua_metalness         = GeometryIn.gua_varying_metalness;
  gua_emissivity        = GeometryIn.gua_varying_emissivity;
  gua_flags_passthrough = false;
  gua_alpha             = 1.0;

  // normal mode or high fidelity shadows
  //if( gl_FrontFacing == true) {
    if (gua_rendering_mode != 1) {
      @material_method_calls_frag@
    }

 /* } else {
    gua_color = vec3(0.0, 1.0, 0.0);
    gua_roughness = 1.0;
    gua_metalness = 0.0;
    gua_emissivity = 1.0;
  }*/

 // gua_color             = vec3( (gua_tangent + vec3(1.0, 1.0, 1.0) / vec3(2.0, 2.0, 2.0) ) ) ;
  submit_fragment(gl_FragCoord.z);

}
