@include "common/header.glsl"

layout(location=0) in vec3 gua_in_position;
layout(location=1) in vec2 gua_in_texcoords;
layout(location=2) in vec3 gua_in_normal;
layout(location=3) in vec3 gua_in_tangent;
layout(location=4) in vec3 gua_in_bitangent;

@include "common/gua_camera_uniforms.glsl"

//@material_uniforms@

//@include "common/gua_vertex_shader_output.glsl"

@include "common/gua_global_variable_declaration.glsl"

@material_method_declarations_vert@

out VertexData {
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
} VertexOut;

void main() {

  //@material_input@

  gua_world_position = (gua_model_matrix * vec4(gua_in_position, 1.0)).xyz;
  gua_view_position  = (gua_model_view_matrix * vec4(gua_in_position, 1.0)).xyz;
  gua_normal         = (gua_normal_matrix * vec4(gua_in_normal, 0.0)).xyz;
  gua_tangent        = (gua_normal_matrix * vec4(gua_in_tangent, 0.0)).xyz;
  gua_bitangent      = (gua_normal_matrix * vec4(gua_in_bitangent, 0.0)).xyz;
  gua_texcoords      = gua_in_texcoords;
  gua_metalness      = 0.01;
  gua_roughness      = 0.1;
  gua_emissivity     = 0;

  //@material_method_calls_vert@

  //@include "common/gua_varyings_assignment.glsl"
  VertexOut.gua_varying_world_position = gua_world_position;
  VertexOut.gua_varying_view_position  = gua_view_position;
  VertexOut.gua_varying_normal         = gua_normal;
  VertexOut.gua_varying_tangent        = gua_tangent;
  VertexOut.gua_varying_bitangent      = gua_bitangent;
  VertexOut.gua_varying_texcoords      = gua_texcoords;
  VertexOut.gua_varying_color          = gua_color;
  VertexOut.gua_varying_roughness      = gua_roughness;
  VertexOut.gua_varying_metalness      = gua_metalness;
  VertexOut.gua_varying_emissivity     = gua_emissivity;

}
