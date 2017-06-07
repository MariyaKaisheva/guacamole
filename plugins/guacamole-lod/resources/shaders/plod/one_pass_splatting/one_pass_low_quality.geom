@include "common/header.glsl"

///////////////////////////////////////////////////////////////////////////////
// input
///////////////////////////////////////////////////////////////////////////////
layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

in VertexData {
  vec3 pass_normal;
  vec3 pass_color;
  float pass_radius;
} VertexIn[];

///////////////////////////////////////////////////////////////////////////////
// output
///////////////////////////////////////////////////////////////////////////////
out VertexData {
  vec2 pass_uv_coords;
  vec3 pass_normal;
  vec3 pass_world_position;
  vec3 pass_color;
  float pass_radius;
} VertexOut;

@include "common/gua_vertex_shader_output.glsl"

///////////////////////////////////////////////////////////////////////////////
// general uniforms
///////////////////////////////////////////////////////////////////////////////
@include "common/gua_camera_uniforms.glsl"

uniform bool enable_backface_culling;
uniform mat4 inverse_transpose_model_view_matrix;
uniform float radius_scaling;
uniform float max_surfel_radius;

const float index_arr[8] = {-1.0, -1.0, 1.0, 1.0, -1.0, 1.0, -1.0, 1.0};

///////////////////////////////////////////////////////////////////////////////
// main program
///////////////////////////////////////////////////////////////////////////////
void main() 
{
  vec3 ms_n = normalize(VertexIn[0].pass_normal); 
  vec4 object_normal = vec4(ms_n, 0.0);
  vec4 view_normal  = inverse_transpose_model_view_matrix * object_normal;

  // do not emit primitives for backfaces 
  if (enable_backface_culling && view_normal.z < 0.0) {
    return;
  } 

  vec4 world_normal = gua_normal_matrix * object_normal;

  // if backface culling is disabled, force front-facing normals
  if (view_normal.z < 0.0) {
    world_normal *= -1.0;
  }

  // compute tangents 
  vec3 ms_u = normalize(vec3((-ms_n.y-ms_n.y) * ms_n.z, ms_n.z * ms_n.x, ms_n.y * ms_n.x));
  vec3 ms_v = cross(ms_n, ms_u);

  // clamp splat size to max. surfel size
  float splat_size = clamp(radius_scaling * VertexIn[0].pass_radius, 0.0, max_surfel_radius);
  ms_u *= splat_size;
  ms_v *= splat_size;

  // helper matrix for triangle strip generation
  mat3x3 step_uv = mat3x3(gl_in[0].gl_Position.xyz,
                          ms_u,
                          ms_v);


  // generate triangle strip
  //  idx2(-1/1)----------idx3(1/1)
  //    |       \              |
  //    |           \          |
  //    |               \      |
  //  idx0(-1/-1)---------idx1(1/-1)

  for(int idx = 0; idx < 4; ++idx ) 
  {
    vec2 uv_coords                  = vec2(index_arr[idx], index_arr[idx + 4]);
    vec3 uv_multiplier              = vec3(1.0, uv_coords);

    VertexOut.pass_uv_coords        = uv_coords;

    vec4 q_pos_ms                   = vec4(step_uv * uv_multiplier, 1.0);
    gl_Position                     = gua_model_view_projection_matrix * q_pos_ms;

    VertexOut.pass_world_position = (gua_model_matrix * q_pos_ms).xyz;
    VertexOut.pass_normal = normalize(world_normal.xyz);
    VertexOut.pass_color = VertexIn[0].pass_color;
    VertexOut.pass_radius = VertexIn[0].pass_radius;
    EmitVertex();
  }
  EndPrimitive();
}

