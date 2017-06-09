@include "common/header.glsl"

///////////////////////////////////////////////////////////////////////////////
// general uniforms
///////////////////////////////////////////////////////////////////////////////
@include "common/gua_camera_uniforms.glsl"


layout (triangles) in;
layout (line_strip, max_vertices = 42) out;


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

float shrink_factor = 0.45;

bool is_front_facing() {
  vec3 cam_to_tri = normalize(VertexIn[0].gua_varying_world_position - gua_camera_position_4.xyz);  
  return dot(VertexIn[0].gua_varying_normal, cam_to_tri) > 0.0;

}

void main() {

	int num_lines = 3; 
	float section_lenght = 1.0/num_lines;

	//vec3 centroid_position = (VertexIn[0].gua_varying_view_position + VertexIn[1].gua_varying_view_position + VertexIn[2].gua_varying_view_position)/3.0;
  vec3 triangle_edges[2];
  triangle_edges[0] = VertexIn[1].gua_varying_view_position - VertexIn[0].gua_varying_view_position;
  triangle_edges[1] = VertexIn[2].gua_varying_view_position - VertexIn[0].gua_varying_view_position;

  for (int line_iterator = 1; line_iterator <= num_lines; ++line_iterator){
  	float shift_step = section_lenght* line_iterator;
  	for(int line_vertex_iterator = 0; line_vertex_iterator < 2; ++line_vertex_iterator){
  		GeometryOut.gua_varying_world_position = VertexIn[line_vertex_iterator].gua_varying_world_position;
	    //GeometryOut.gua_varying_view_position = VertexIn[line_vertex_iterator].gua_varying_view_position;

	    GeometryOut.gua_varying_normal = VertexIn[line_vertex_iterator].gua_varying_normal;  
		

		GeometryOut.gua_varying_tangent = VertexIn[line_vertex_iterator].gua_varying_tangent;
		GeometryOut.gua_varying_bitangent = VertexIn[line_vertex_iterator].gua_varying_bitangent;
		GeometryOut.gua_varying_texcoords = VertexIn[line_vertex_iterator].gua_varying_texcoords;

		GeometryOut.gua_varying_color = VertexIn[line_vertex_iterator].gua_varying_color;
		GeometryOut.gua_varying_roughness = VertexIn[line_vertex_iterator].gua_varying_roughness;
		GeometryOut.gua_varying_metalness = VertexIn[line_vertex_iterator].gua_varying_metalness;
		GeometryOut.gua_varying_emissivity = VertexIn[line_vertex_iterator].gua_varying_emissivity;
		
		vec3 line_vertex_position; 
		
		line_vertex_position = VertexIn[0].gua_varying_view_position + shift_step*triangle_edges[line_vertex_iterator];

		gl_Position = gua_projection_matrix * vec4(line_vertex_position , 1.0);
		//gl_Position = gua_projection_matrix * vec4(VertexIn[index].gua_varying_view_position , 1.0);
		EmitVertex();
  	}
	EndPrimitive();
  }

  



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