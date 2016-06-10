@include "common/header.glsl"

///////////////////////////////////////////////////////////////////////////////
// general uniforms
///////////////////////////////////////////////////////////////////////////////
@include "common/gua_camera_uniforms.glsl"


layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;


in VertexData {
  //input from vertex shader
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
} VertexIn[];


out VertexData{
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
} GeometryOut;

float shrink_factor = 0.95;

bool is_front_facing() {
  //vec3 zero_to_one = normalize(VertexIn[1].gua_varying_view_position - VertexIn[0].gua_varying_view_position) ;
  //vec3 zero_to_two = normalize(VertexIn[2].gua_varying_view_position - VertexIn[0].gua_varying_view_position ) ;

  vec3 cam_to_tri = normalize(VertexIn[0].gua_varying_world_position - gua_camera_position_4.xyz);

  
  return dot(VertexIn[0].gua_varying_normal, cam_to_tri) > 0.0;

}

void main() {



	vec3 centroid_position = (VertexIn[0].gua_varying_view_position + VertexIn[1].gua_varying_view_position + VertexIn[2].gua_varying_view_position)/3.0;
  for (int index = 0; index < 3; ++index){
    GeometryOut.gua_varying_world_position = VertexIn[index].gua_varying_world_position;
    GeometryOut.gua_varying_view_position = VertexIn[index].gua_varying_view_position;

    if( is_front_facing() ) {
      GeometryOut.gua_varying_normal = -VertexIn[index].gua_varying_normal;
    } else {
      GeometryOut.gua_varying_normal = VertexIn[index].gua_varying_normal;    	
    }
	GeometryOut.gua_varying_tangent = VertexIn[index].gua_varying_tangent;
	GeometryOut.gua_varying_bitangent = VertexIn[index].gua_varying_bitangent;
	GeometryOut.gua_varying_texcoords = VertexIn[index].gua_varying_texcoords;
	GeometryOut.gua_varying_color = VertexIn[index].gua_varying_color;
	GeometryOut.gua_varying_roughness = VertexIn[index].gua_varying_roughness;
	GeometryOut.gua_varying_metalness = VertexIn[index].gua_varying_metalness;
	GeometryOut.gua_varying_emissivity = VertexIn[index].gua_varying_emissivity;
	
	vec3 modified_position; 
	
    if( is_front_facing() ) {
      modified_position = centroid_position + 1.05*(VertexIn[index].gua_varying_view_position - centroid_position);
    } else {
      modified_position = centroid_position + 1.0*(VertexIn[index].gua_varying_view_position - centroid_position);
    }

	gl_Position = gua_projection_matrix * vec4(modified_position , 1.0);
	//gl_Position = gua_projection_matrix * vec4(VertexIn[index].gua_varying_view_position , 1.0);
	EmitVertex();
  }

  EndPrimitive();



  //}

}
/*
@include "common/header.glsl"

#extension GL_EXT_geometry_shader4: enable
//layout (early_fragment_tests) in;

//@include "common/gua_fragment_shader_input.glsl"
//@include "common/gua_camera_uniforms.glsl"

//uniform float gua_texel_width;
//uniform float gua_texel_height;



//@material_uniforms@

//@include "common/gua_global_variable_declaration.glsl"

//@material_method_declarations_frag@


layout (triangles) in;
layout (triangle_strip, max_vertices = 32) out;

in VertexData {
  //input from vertex shader
 vec3  gua_varying_world_position;
 vec3  gua_varying_normal;
 vec3  gua_varying_tangent;
 vec3  gua_varying_bitangent;
 vec2  gua_varying_texcoords;
 vec3  gua_varying_color;
 float gua_varying_roughness;
 float gua_varying_metalness;
 float gua_varying_emissivity;
} VertexIn[];


out GeometryData{
 //output for fragment shader
vec3  gua_varying_world_position;
vec3  gua_varying_normal;
vec3  gua_varying_tangent;
vec3  gua_varying_bitangent;
vec2  gua_varying_texcoords;
vec3  gua_varying_color;
float gua_varying_roughness;
float gua_varying_metalness;
float gua_varying_emissivity;
} GeometryOut;



void main() {

	for (int index = 0; index < 3; ++index){
		GeometryOut.gua_varying_world_position = VertexIn[index].gua_varying_world_position;
		GeometryOut.gua_varying_normal = VertexIn[index].gua_varying_normal;
		GeometryOut.gua_varying_tangent = VertexIn[index].gua_varying_tangent;
		GeometryOut.gua_varying_bitangent = VertexIn[index].gua_varying_bitangent;
		GeometryOut.gua_varying_texcoords = VertexIn[index].gua_varying_texcoords;
		GeometryOut.gua_varying_color = VertexIn[index].gua_varying_color;
		GeometryOut.gua_varying_roughness = VertexIn[index].gua_varying_roughness;
		GeometryOut.gua_varying_metalness = VertexIn[index].gua_varying_metalness;
		GeometryOut.gua_varying_emissivity = VertexIn[index].gua_varying_emissivity;

		gl_Position = gua_projection_matrix * vec4(VertexIn[index].gua_varying_world_position, 1.0);
		EmitVertex();
	}

  EndPrimitive();
}
*/