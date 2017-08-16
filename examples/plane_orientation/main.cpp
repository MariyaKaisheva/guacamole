/******************************************************************************
 * guacamole - delicious VR                                                   *
 *                                                                            *
 * Copyright: (c) 2011-2013 Bauhaus-Universität Weimar                        *
 *                                                                            *
 * This program is free software: you can redistribute it and/or modify it    *
 * under the terms of the GNU General Public License as published by the Free *
 * Software Foundation, either version 3 of the License, or (at your option)  *
 * any later version.                                                         *
 *                                                                            *
 * This program is distributed in the hope that it will be useful, but        *
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License   *
 * for more details.                                                          *
 *                                                                            *
 * You should have received a copy of the GNU General Public License along    *
 * with this program. If not, see <http://www.gnu.org/licenses/>.             *
 *                                                                            *
 ******************************************************************************/


#include <lamure/npr/core.h>

#include <scm/input/tracking/art_dtrack.h>
#include <scm/input/tracking/target.h>
#include <scm/core/math/quat.h>


#include <gua/guacamole.hpp>
#include <gua/utils/Trackball.hpp>
#include <gua/math/math.hpp>
#include <gua/math/BoundingBox.hpp>

#include <gua/node/PLodNode.hpp>
#include <gua/node/TriMeshNode.hpp>
#include <gua/node/Node.hpp>
#include <gua/renderer/LodLoader.hpp>
#include <gua/renderer/PBSMaterialFactory.hpp>
#include <gua/renderer/PLodPass.hpp>
#include <gua/renderer/LightVisibilityPass.hpp>
#include <gua/renderer/PLodPass.hpp>
#include <gua/renderer/TexturedQuadPass.hpp>
#include <gua/renderer/DebugViewPass.hpp>
#include <gua/renderer/BBoxPass.hpp>

#include <boost/program_options.hpp>

#include <fstream>

#define USE_SIDE_BY_SIDE 1
#define USE_ANAGLYPH 0
#define USE_MONO 0

#define TRACKING_ENABLED 1
#define USE_LOW_RES_WORKSTATION 0

// global variables
bool close_window = false;
bool reset_scene_position = false;
bool freeze_cut_update = false; //for lod models
bool manipulate_scene_geometry = false;
bool show_lense = true;
bool snap_plane_to_model = false; 
std::vector<gua::math::BoundingBox<gua::math::vec3> > scene_bounding_boxes;
auto zoom_factor = 1.0;
float screen_width = 0.595f;
float screen_height = 0.3346f;
gua::math::mat4 current_probe_tracking_matrix(gua::math::mat4::identity());
double x_offset_scene_track_target = 0.0;
double y_offset_scene_track_target = 0.0;
double z_offset_scene_track_target = 0.0;

std::vector<std::string> model_filenames; 
uint8_t model_idex = 0;
bool available_preview_trimesh = false;

int32_t depth = 4;
bool write_obj_file = true;
bool use_nurbs = true;
bool apply_alpha_shapes = true;
uint32_t max_number_line_loops = 20;
bool is_line_preview_node_active = false;
std::string trimesh_preview_filename = "";
std::string data_source = "/home/vajo3185/Programming/guacamole/examples/plane_orientation/";

float error_threshold = 3.7f; //for point cloud models
float radius_scale = 0.7f;  //for point cloud models

std::string plod_geometry_parent = "/model_translation/model_rotation/model_scaling/geometry_root";
std::string trimesh_preview_geometry_parent = "/model_translation/model_rotation/model_scaling/preview_trimesh_root";

float test_sphere_radius = 0.25f;
auto plod_pass = std::make_shared<gua::PLodPassDescription>();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// forward mouse interaction to trackball
void mouse_button(gua::utils::Trackball& trackball,
                  int mousebutton,
                  int action,
                  int mods) {
  gua::utils::Trackball::button_type button;
  gua::utils::Trackball::state_type state;

  switch (mousebutton) {
    case 0:
      button = gua::utils::Trackball::left;
      break;
    case 2:
      button = gua::utils::Trackball::middle;
      break;
    case 1:
      button = gua::utils::Trackball::right;
      break;
  };

  switch (action) {
    case 0:
      state = gua::utils::Trackball::released;
      break;
    case 1:
      state = gua::utils::Trackball::pressed;
      break;
  };

  trackball.mouse(button, state, trackball.posx(), trackball.posy());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void remove_old_preview(gua::SceneGraph & graph){
  if(is_line_preview_node_active) {
    //remove old line preview
    graph.remove_node(trimesh_preview_geometry_parent + "/line_preview");
    is_line_preview_node_active = false;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void toggle_visibility(gua::SceneGraph & graph){
    auto all_geometry_nodes = graph[plod_geometry_parent]->get_children();
    uint8_t num_models = all_geometry_nodes.size();
    for(uint8_t node_index  = 0; node_index < num_models; ++node_index){
      auto& node = all_geometry_nodes[node_index];
      if(!node->get_tags().has_tag("invisible")){
        //hide currently visible model
        node->get_tags().add_tag("invisible");
          //make next model visible
          if(node_index == num_models - 1){
            all_geometry_nodes[0]->get_tags().remove_tag("invisible");
            model_idex = 0;
          }else{
            all_geometry_nodes[node_index + 1]->get_tags().remove_tag("invisible");
            model_idex = node_index + 1; 
          }

          remove_old_preview(graph);

        break;
      }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::string extract_name(std::string full_name){
    std::string model_filename_without_path = full_name.substr(full_name.find_last_of("/\\") + 1); 
    std::string model_filename_without_path_and_extension = model_filename_without_path.substr(0, model_filename_without_path.size() - 4);

    return model_filename_without_path_and_extension;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float get_rotation_angle(scm::math::mat4f rot_mat){
   scm::math::quat<float> output_quat;
   output_quat = scm::math::quat<float>::from_matrix(rot_mat);
   float angle; 
   scm::math::vec3f axis; 
   output_quat.retrieve_axis_angle(angle, axis);

    return angle;
}

//TODO clean duplicat code!
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void write_rot_mat (gua::SceneGraph const& graph){
  std::string path_to_plane_rot_node = "/model_translation/model_rotation/model_scaling/plane_root/plane_translation/plane_rotation";
  auto rot_mat =  gua::math::get_rotation(graph[path_to_plane_rot_node]->get_transform());
  std::cout << rot_mat << std::endl; 
  scm::math::quat<double> output_quat;
  output_quat = scm::math::quat<double>::from_matrix(rot_mat);
  double angle; 
  scm::math::vec3d axis; 
  output_quat.retrieve_axis_angle(angle, axis);
  std::cout << "angle: " << angle << std::endl;
  std::cout << "axis:  " << axis.x << " " << axis.y << " " << axis.z << std::endl;

  std::string  model_name;
  auto all_geometry_nodes = graph[plod_geometry_parent]->get_children();
  for(auto& node : all_geometry_nodes){
    if(!node->get_tags().has_tag("invisible")){
      model_name = node->get_name();
    }
    
  }

  std::cout << "model_name: " << model_name << std::endl;

  std::string output_filename = "/mnt/pitoti/MA_MK/rot_mat_files/model_" 
                                + model_name
                                + "_angle_"
                                + std::to_string(angle) 
                                + ".rot";

  std::ofstream output_file(output_filename);
 
  if (output_file.is_open()){
    output_file << angle << " " << axis.x << " " << axis.y << " " << axis.z;
    output_file.close();
  }
  else{
    std::cout << "Cannot open output file to write to! \n";
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool call_LA_preview(gua::SceneGraph const& graph){

  std::string path_to_plane_rot_node = "/model_translation/model_rotation/model_scaling/plane_root/plane_translation/plane_rotation";
  scm::math::mat4f rot_mat =  scm::math::mat4f(gua::math::get_rotation(graph[path_to_plane_rot_node]->get_transform()));
  std::string bvh_filename = model_filenames[model_idex];
  std::string model_filename_without_path_and_extension = extract_name(bvh_filename);
  auto angle = get_rotation_angle(rot_mat);  
  std::string output_filename_without_extension  =  model_filename_without_path_and_extension
                                                     + "_d" + std::to_string(depth)
                                                     + "_angle_" + std::to_string(angle);

  trimesh_preview_filename =  data_source + output_filename_without_extension + ".obj";
  npr::core::generate_line_art(rot_mat,
                               bvh_filename, 
                               depth,
                               write_obj_file, 
                               use_nurbs, 
                               apply_alpha_shapes,
                               output_filename_without_extension,
                               max_number_line_loops);
  std::cout << "DONE CALLING THE LAMURE LIB FUNCTION\n";
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void scale_scene (double zoom_factor, gua::SceneGraph& graph){
  auto scaling_mat = scm::math::make_scale(zoom_factor, zoom_factor, zoom_factor);
  graph["/model_translation/model_rotation/model_scaling"]->set_transform(scaling_mat);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void build_pipe (gua::PipelineDescription& pipe){
  pipe.clear();
  pipe.add_pass(std::make_shared<gua::TriMeshPassDescription>());
  pipe.add_pass(std::make_shared<gua::PLodPassDescription>());
  pipe.add_pass(std::make_shared<gua::LightVisibilityPassDescription>());
  pipe.add_pass(std::make_shared<gua::ResolvePassDescription>()); 
  //pipe.add_pass(std::make_shared<gua::BBoxPassDescription>());
 //pipe.add_pass(std::make_shared<gua::DebugViewPassDescription>());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//key-board interactions
void key_press(gua::PipelineDescription& pipe, 
         gua::SceneGraph& graph, 
         int key,
         int scancode, 
         int action, 
         int mods){
  //std::cout << scancode << "scancode\n";
 
  if(9 == scancode){ //'Esc'
    close_window = true;
  }

  if(51 == scancode && action == 1){ // '\'
    reset_scene_position = true;
  }

  //toggle what geometry is manipulated by the tracked cubic probe
  //switches between scene geometry and slicing plane
  if(65 == scancode && action == 1){ //'Spacebar'
    manipulate_scene_geometry = !manipulate_scene_geometry;
  }

  //toggle linked position tracking (model & slicing plane moving together or not)
  if(39 == scancode && action == 1){ //'s'
    snap_plane_to_model = !snap_plane_to_model;
  }

  if (action == 0) return;
  switch(std::tolower(key)){
    case '-':
      if(zoom_factor > 0.06){
        zoom_factor -= 0.05;
      }
      scale_scene(zoom_factor, graph);
      break;

    case '=':
      zoom_factor += 0.05;
      scale_scene(zoom_factor, graph);
      break;

     case 'u':
        if(radius_scale <= 20.0){
          radius_scale += 0.05;
        }
      break;

    case 'j':
        if(radius_scale >= 0.07){
          radius_scale -= 0.05;
        }
      break;

    case 'z':
        if(error_threshold <= 20.0){
          error_threshold += 1.0;
        }
      break;

    case 'x':
        if(error_threshold >= 0.2){
          error_threshold -= 0.19;
        }
      break;

    case 'f':
        freeze_cut_update = !freeze_cut_update;
      break;

    case 'i':
      show_lense =! show_lense;
      break;

    case 'p':
      available_preview_trimesh = call_LA_preview(graph);
      break;


    //write rotation matrix out
    case 'w': 
      write_rot_mat(graph);
      break;

    //toggle visibility of Plod models 
    case 'q': 
      toggle_visibility(graph);
      break;


    //decrease blending sphere radius
    case 'n':
        if(test_sphere_radius > 0.005f){
          test_sphere_radius -= 0.005f;
        }
      break;
    //increase blending sphere radius
    case 'm':
        if(test_sphere_radius <= 0.95f){
          test_sphere_radius += 0.05f;
        }
      break; 
  
    default:
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::pair<std::vector<std::string>, std::vector<scm::math::mat4f>>
interpret_config_file(std::string const&  path_to_file) {
  namespace fs = boost::filesystem;
  std::vector<scm::math::mat4f> model_transformations;
  std::vector<std::string> model_filenames;
  auto file_name = fs::canonical(path_to_file); //TODO check if this is needed elsewhere as well
  std::ifstream resource_file(file_name.string()); 

  std::string single_file_line;
  if (resource_file.is_open()){
    while (std::getline(resource_file, single_file_line)){
      std::cout << single_file_line << "\n";
      model_filenames.push_back(single_file_line);
      model_transformations.push_back(scm::math::mat4f::identity());
    }
    resource_file.close();
  }

  return std::make_pair(model_filenames, model_transformations);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void add_models_to_graph(std::vector<std::string> const& model_files, 
                         gua::SceneGraph& graph,
                         std::vector<scm::math::mat4f> const& model_transformations){
  gua::LodLoader lod_loader;

  for (auto const& model : model_files) {

    auto plod_node = lod_loader.load_lod_pointcloud(model);
    std::string model_filename_without_path_and_extension = extract_name(model);
    plod_node->set_name(model_filename_without_path_and_extension);
    graph.add_node(plod_geometry_parent, plod_node); 
    scene_bounding_boxes.push_back(plod_node->get_bounding_box()); 
    plod_node->set_draw_bounding_box(true);
    model_filenames.push_back(model);
  }

    auto all_geometry_nodes = graph[plod_geometry_parent]->get_children();

    //first: flag all geometry nodes invisible
    for(auto& node : all_geometry_nodes){

      auto scene_bbox = node->get_bounding_box();
      auto size_along_x = scene_bbox.size(0);
      auto size_along_y = scene_bbox.size(1);
      auto size_along_z = scene_bbox.size(2);
      auto longest_axis = std::max(std::max(size_along_x, size_along_y), std::max(size_along_y, size_along_z));
      float scaling_factor = screen_width / (longest_axis);
      node->translate(-scene_bbox.center().x, -scene_bbox.center().y, -scene_bbox.center().z);
      node->scale(scaling_factor);
      node->set_draw_bounding_box(true);
      node->get_tags().add_tag("invisible");

    }

    //then: remove invisibility flag from first geometry node 
    all_geometry_nodes[0]->get_tags().remove_tag("invisible");

    graph[plod_geometry_parent]->set_draw_bounding_box(true);   
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv) {
  // initialize guacamole
  gua::init(1, argv);

  //setup scene ////////////////////////////////////
  gua::SceneGraph graph("main_scenegraph");
  //auto scene_root_node = graph.add_node<gua::node::TransformNode>("/", "scene_root");
  auto model_translation_node = graph.add_node<gua::node::TransformNode>("/", "model_translation");
  auto model_rotation_node = graph.add_node<gua::node::TransformNode>(model_translation_node, "model_rotation");
  auto model_scaling_node = graph.add_node<gua::node::TransformNode>(model_rotation_node, "model_scaling");
  auto geometry_root = graph.add_node<gua::node::TransformNode>(model_scaling_node, "geometry_root");
  auto preview_trimesh_root = graph.add_node<gua::node::TransformNode>(model_scaling_node, "preview_trimesh_root");
  auto plane_root = graph.add_node<gua::node::TransformNode>(model_scaling_node, "plane_root");

  //light source 
  auto light_pointer = graph.add_node<gua::node::TransformNode>("/", "light_pointer");
  auto light_source = graph.add_node<gua::node::LightNode>("/light_pointer", "light_source");
  light_source->data.set_type(gua::node::LightNode::Type::SUN);
  light_source->data.brightness = 3.0f;

  //slicing plane geometry
  auto plane_translation_node = graph.add_node<gua::node::TransformNode>(plane_root, "plane_translation");
  auto plane_rotation_node = graph.add_node<gua::node::TransformNode>(plane_translation_node, "plane_rotation");
  auto plane_scaling_node = graph.add_node<gua::node::TransformNode>(plane_rotation_node, "plane_scaling");
  auto plane_geometry_root = graph.add_node<gua::node::TransformNode>(plane_scaling_node, "plane_geometry_root");

  gua::TriMeshLoader   trimesh_loader;

  auto line_rendering_material_w_back_faces(gua::MaterialShaderDatabase::instance()
                                ->lookup("gua_default_material")
                                ->make_new_material());

  line_rendering_material_w_back_faces->set_show_back_faces(true);
  line_rendering_material_w_back_faces->set_render_wireframe(true);
  line_rendering_material_w_back_faces->set_uniform("Emissivity", 1.0f);
  line_rendering_material_w_back_faces->set_uniform("Color", gua::math::vec4f(1.0f, 0.0f, 0.0f, 1.0f) );


  auto plane_material(gua::MaterialShaderDatabase::instance()
                                ->lookup("gua_default_material")
                                ->make_new_material());
  plane_material->set_show_back_faces(true);
  plane_material->set_render_wireframe(false);
  plane_material->set_uniform("Emissivity", 1.0f);
  plane_material->set_uniform("Color", gua::math::vec4f(0.15f, 0.1f, 0.3f, 0.5f) );

  auto plane_geometry(trimesh_loader.create_geometry_from_file(
                "ring",
                "/home/vajo3185/Programming/guacamole/examples/plane_orientation/data/objects/plane.obj", 
                plane_material,
                gua::TriMeshLoader::NORMALIZE_POSITION |
                gua::TriMeshLoader::NORMALIZE_SCALE ));


  //define screen resolution
  gua::math::vec2ui resolution;
  #if !USE_LOW_RES_WORKSTATION
    resolution = gua::math::vec2ui(2560, 1440);
  #else
    resolution = gua::math::vec2ui(1920, 1080);
  #endif

  //screen related variables
  //physical set-up for SBS stereo workstation 
  //TODO: value assigment from inout file
  float screen_width = 0.595f;
  float screen_height = 0.3346f;
  float screen_center_offset_x = 3.88f;
  float screen_center_offset_y = 1.3553f;
  float screen_center_offset_z = -0.49f;
  float screen_rotation_x = -12.46f;
  float screen_rotation_y = -90.0f;
  //float screen_rotation_z = 0.0f;
  float distance_to_screen =  0.6f;

  float eye_dist = 0.06f;
  float glass_eye_offset = 0.03f;

  int   window_width = 2560;
  int   window_height = 1440;

  //physical setup of output viewport ///////////////
  auto screen = graph.add_node<gua::node::ScreenNode>("/", "screen");
  screen->data.set_size(gua::math::vec2(screen_width, screen_height));
    
  #if USE_SIDE_BY_SIDE 
  screen->rotate(screen_rotation_x, 1, 0, 0);
  screen->rotate(screen_rotation_y, 0, 1, 0);
  //screen->rotate(screen_rotation_z, 0, 0, 1);
  screen->translate(screen_center_offset_x, screen_center_offset_y, screen_center_offset_z);
 // #else
  //screen->translate(0, 0, -0.6);
  #endif

  //camera configuration ///////////////////////////
  #if USE_SIDE_BY_SIDE 
  auto navigation_eye_offset = graph.add_node<gua::node::TransformNode>("/", "eye_offset");
  auto camera = graph.add_node<gua::node::CameraNode>("/eye_offset", "cam");
  camera->translate(screen_center_offset_x - distance_to_screen, screen_center_offset_y, screen_center_offset_z);
  navigation_eye_offset->translate(0, 0, glass_eye_offset);
  #else 
  auto camera = graph.add_node<gua::node::CameraNode>("/", "cam");
  camera->translate(0.0, 0.0, distance_to_screen);
  #endif
  #if USE_MONO 
  camera->config.set_enable_stereo(false);
  #else
  camera->config.set_enable_stereo(true);
  #endif
  camera->config.set_resolution(resolution);
  camera->config.set_screen_path("/screen");
  camera->config.set_scene_graph_name("main_scenegraph");
  camera->config.set_output_window_name("main_window");
  camera->config.set_near_clip(0.02);
  camera->config.set_far_clip(3000.0);
  camera->config.set_eye_dist(eye_dist);
  camera->config.mask().blacklist.add_tag("invisible");

  //////////////////input handling addapted from Lamure /////////////
  namespace po = boost::program_options;
 
  std::string resource_file = "auto_generated.rsc";
  po::options_description desc("options: ");
  desc.add_options()
    ("help,h", "print help message")
    ("input,f", po::value<std::string>(), "specify input file with scene configuration")
    ("models,m", po::value <std::vector<std::string>>()->multitoken(), "list paths to desired input models")
    ("transformations,t", po::value<std::string>(), "specify input file with transformation data")
  ;

  po::variables_map vm;
  std::vector<scm::math::mat4f> dummy_transformations;

  try {    
      auto parsed_options = po::command_line_parser(argc, argv).options(desc).run();
      po::store(parsed_options, vm);
      po::notify(vm);
      bool no_input = !vm.count("input") && !vm.count("models");

      if (vm.count("help") || no_input)
      {
        std::cout << desc;
        return 0;
      }

      //single scene decription file is provided
      if (vm.count("input")){
        auto model_atributes = interpret_config_file(vm["input"].as<std::string>());
        add_models_to_graph(model_atributes.first, graph, model_atributes.second);  
      } else if (vm.count("models")){
        add_models_to_graph(vm["models"].as<std::vector<std::string>>(), graph, dummy_transformations);  
      } else {
        //TODO this case shoud be excluded based on previous if-statements 
        return 0;
      }

    } catch (std::exception& e) {
      std::cout << "Warning: No input file specified. \n" << desc;
      return 0;
    }
  ///////////////////////////////////////////////////
  #if USE_SIDE_BY_SIDE
  auto initial_scene_translation_mat = gua::math::mat4(scm::math::make_translation(screen_center_offset_x, 
                                                                                  screen_center_offset_y, 
                                                                                  screen_center_offset_z));
  auto initial_scene_rotation_mat = gua::math::mat4(scm::math::make_rotation(0.0, 1.0, 0.0, 0.0)); 
  auto initial_scene_scaling_mat = gua::math::mat4(scm::math::make_scale(1.0, 1.0, 1.0));
  #else
  auto initial_scene_translation_mat = gua::math::mat4(scm::math::make_translation(0.0, 0.0, -0.18));
  auto initial_scene_rotation_mat = gua::math::mat4(scm::math::make_rotation(-45.0, 1.0, 0.0, 0.0)); 
  auto initial_scene_scaling_mat  = gua::math::mat4(scm::math::make_scale(0.15, 0.15, 0.15));
  #endif

  
  model_translation_node->set_transform(initial_scene_translation_mat);
  model_rotation_node->set_transform(initial_scene_rotation_mat);
  model_scaling_node->set_transform(initial_scene_scaling_mat);

  graph.add_node(plane_geometry_root, plane_geometry);
  /*
  std::shared_ptr<gua::node::TriMeshNode> line_preview_trimesh_node = nullptr; //trimesh node for line preview
  line_preview_trimesh_node->get_tags().add_tag("invisible");
  graph.add_node(geometry_root, line_preview_trimesh_node);
  */
 
  //add pipeline with rendering passes
  auto pipe = std::make_shared<gua::PipelineDescription>();
  build_pipe(*pipe);
  pipe->set_enable_abuffer(true);

  camera->set_pipeline_description(pipe);

  //add mouse interaction
  gua::utils::Trackball trackball(0.002, 0.0002, 0.05); 

  //window configuration/////////////////////////////
  auto window = std::make_shared<gua::GlfwWindow>();
  gua::WindowDatabase::instance()->add("main_window", window);
  window->config.set_enable_vsync(true);
  window->config.set_size(resolution);
  window->config.set_resolution(resolution);

  window->on_move_cursor.connect([&](gua::math::vec2 const& pos){
                trackball.motion(pos.x, pos.y);
              });
  
  window->on_button_press.connect(std::bind(mouse_button, 
      std::ref(trackball),
      std::placeholders::_1,
      std::placeholders::_2, 
      std::placeholders::_3));

  window->on_key_press.connect(std::bind(key_press,
        std::ref(*(camera->get_pipeline_description())),
        std::ref(graph),
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3,
        std::placeholders::_4));

  #if USE_SIDE_BY_SIDE
    window->config.set_stereo_mode(gua::StereoMode::SIDE_BY_SIDE);
    window->config.set_fullscreen_mode(true);
    window->config.set_size(gua::math::vec2ui(2*window_width, window_height));
    window->config.set_right_position(gua::math::vec2ui(resolution[0], 0));
    window->config.set_right_resolution(resolution);
    window->config.set_left_position(gua::math::vec2ui(0, 0));
    window->config.set_left_resolution(resolution);
  #endif

  #if USE_ANAGLYPH
    window->config.set_stereo_mode(gua::StereoMode::ANAGLYPH_RED_CYAN);
  #endif

  #if USE_MONO
    window->config.set_stereo_mode(gua::StereoMode::MONO);
  #endif

  //add renderer
  gua::Renderer renderer; 

  //tracking //////////////////////////////////////
  #if TRACKING_ENABLED
    gua::math::mat4 current_cam_tracking_matrix(gua::math::mat4::identity());
    //gua::math::mat4 current_ray_tracking_matrix(gua::math::mat4::identity());

    std::thread tracking_thread([&]() {
      scm::inp::tracker::target_container targets;
      targets.insert(scm::inp::tracker::target_container::value_type(5, scm::inp::target(5)));
      targets.insert(scm::inp::tracker::target_container::value_type(22, scm::inp::target(22)));
      //targets.insert(scm::inp::tracker::target_container::value_type(17, scm::inp::target(17)));

      scm::inp::art_dtrack* dtrack(new scm::inp::art_dtrack(5000));
      if (!dtrack->initialize()) {
        std::cerr << std::endl << "Tracking System Fault" << std::endl;
        return;
      }
      while (true) {       
        dtrack->update(targets);
        auto camera_target = targets.find(5)->second.transform();
        camera_target[12] /= 1000.f; 
        camera_target[13] /= 1000.f; 
        camera_target[14] /= 1000.f;
        current_cam_tracking_matrix = camera_target;

        auto scene_root_target = targets.find(22)->second.transform();
        scene_root_target[12] /= 1000.f; 
        scene_root_target[13] /= 1000.f;
        scene_root_target[14] /= 1000.f;
        current_probe_tracking_matrix = scene_root_target;

        /*auto ray_origin_target = targets.find(17)->second.transform();
        ray_origin_target[12] /= 1000.f; 
        ray_origin_target[13] /= 1000.f; 
        ray_origin_target[14] /= 1000.f;
        current_ray_tracking_matrix = ray_origin_target;*/
      }
    });
  #endif

  auto initial_translation_in_tracking_space_coordinates = gua::math::mat4::identity();
  auto initial_rotation_in_tracking_space_coordinates = gua::math::mat4::identity();
  gua::LodLoader lod_loader;
  //application loop //////////////////////////////
  gua::events::MainLoop loop;
  double tick_time = 1.0/500.0;
  gua::events::Ticker ticker(loop, tick_time);
  ticker.on_tick.connect([&](){

    //worker thread creation

    //terminate application
    if (window->should_close() || close_window) {
      renderer.stop();
        window->close();

        //signal worker thread to shut itself down

        loop.stop();
      }
      else {


        if(available_preview_trimesh) {
          
          remove_old_preview(graph);                            
          auto line_preview_trimesh_node(trimesh_loader.create_geometry_from_file(
              "line_preview",
              trimesh_preview_filename, 
              line_rendering_material_w_back_faces,
              gua::TriMeshLoader::NORMALIZE_POSITION |
              gua::TriMeshLoader::NORMALIZE_SCALE ) ) ;

          graph.add_node(preview_trimesh_root, line_preview_trimesh_node);
          is_line_preview_node_active = true;

          //line_preview_trimesh_node->get_tags().remove_tag("invisible");
          available_preview_trimesh = false;
        }

        #if(USE_SIDE_BY_SIDE && TRACKING_ENABLED)
          #if 1
            camera->set_transform(current_cam_tracking_matrix);
          #endif 

          auto current_probe_rotation_mat = gua::math::get_rotation(current_probe_tracking_matrix);
          auto current_probe_translation_mat = scm::math::make_translation(current_probe_tracking_matrix[12], current_probe_tracking_matrix[13], current_probe_tracking_matrix[14]);

          if(!trackball.get_left_button_press_state()){//no transformations applied; user doesn't trigger movement
            if(snap_plane_to_model)  {
              initial_rotation_in_tracking_space_coordinates = scm::math::inverse(current_probe_rotation_mat) *
                                                               (model_rotation_node->get_transform());  

              initial_translation_in_tracking_space_coordinates = scm::math::inverse(current_probe_translation_mat)
                                                                  * (model_translation_node->get_transform());
           }
           else{
              initial_rotation_in_tracking_space_coordinates = scm::math::inverse(current_probe_rotation_mat)
                                                               * (model_rotation_node->get_transform())
                                                               * (plane_rotation_node->get_transform()); 
      
              initial_translation_in_tracking_space_coordinates = scm::math::inverse(current_probe_translation_mat)
                                                                  * (model_translation_node->get_transform())
                                                                  * (model_rotation_node->get_transform())
                                                                  * (plane_translation_node->get_transform());
           }       

          }
          else{//user triggers movement; transformations are apllied to plod and trimesh geometry

            if(snap_plane_to_model){
                auto rotation_mat = current_probe_rotation_mat * initial_rotation_in_tracking_space_coordinates;
                auto translation_mat = current_probe_translation_mat*initial_translation_in_tracking_space_coordinates;
                model_translation_node->set_transform(translation_mat);
                model_rotation_node->set_transform(rotation_mat);

            }else{
                auto rotation_mat = scm::math::inverse(model_rotation_node->get_transform())
                                    * current_probe_rotation_mat
                                    * initial_rotation_in_tracking_space_coordinates;
                auto translation_mat =   scm::math::inverse(model_rotation_node->get_transform())
                                       * scm::math::inverse(model_translation_node->get_transform())
                                       * current_probe_translation_mat
                                       * initial_translation_in_tracking_space_coordinates;
                plane_translation_node->set_transform(translation_mat);
                plane_rotation_node->set_transform(rotation_mat);
            }
          }

          if(reset_scene_position){
            if(snap_plane_to_model){
              model_translation_node->set_transform(initial_scene_translation_mat);
              model_rotation_node->set_transform(initial_scene_rotation_mat);
              scale_scene(1.0, graph);
              zoom_factor = 1.0;
            }
            else{
              plane_translation_node->set_transform(scm::math::inverse(model_translation_node->get_transform())
                                                    * initial_scene_translation_mat);
              plane_rotation_node->set_transform(scm::math::inverse(model_rotation_node->get_transform()) 
                                                 * initial_scene_rotation_mat);
            }
            reset_scene_position = false;
          } 

          double scale_factor = test_sphere_radius*1.5; //75% influence radius 
          plane_scaling_node->set_transform(scm::math::make_scale(scale_factor, scale_factor, scale_factor));

          for(auto& geometry_node : geometry_root->get_children()){
            std::dynamic_pointer_cast<gua::node::PLodNode>(geometry_node)->set_cut_dispatch(freeze_cut_update);
            std::dynamic_pointer_cast<gua::node::PLodNode>(geometry_node)->set_error_threshold(error_threshold);
            std::dynamic_pointer_cast<gua::node::PLodNode>(geometry_node)->set_radius_scale(radius_scale);
          }

          if(show_lense == false){
            plane_geometry_root->get_tags().add_tag("invisible");
          }
          else{
            plane_geometry_root->get_tags().remove_tag("invisible");
          }

        #else //!USE_SIDE_BY_SIDE || !TRACKING_ENABLED
        // apply trackball matrix to object
        gua::math::mat4 modelmatrix = scm::math::make_translation(gua::math::float_t(trackball.shiftx()),
                                                                  gua::math::float_t(trackball.shifty()),
                                                                  gua::math::float_t(trackball.distance())) * 
                                      gua::math::mat4(trackball.rotation());
        if(snap_plane_to_model){
          //scene_root_node->set_transform(modelmatrix);
        }

        if(show_lense){
          plane_translation_node->set_transform(modelmatrix);
          plane_geometry_root->get_tags().remove_tag("invisible");
        }else{
          model_translation_node->set_transform(modelmatrix);
          plane_geometry_root->get_tags().add_tag("invisible");
        }
        

        for(auto& geometry_node : geometry_root->get_children()){
          std::dynamic_pointer_cast<gua::node::PLodNode>(geometry_node)->set_cut_dispatch(freeze_cut_update);
          std::dynamic_pointer_cast<gua::node::PLodNode>(geometry_node)->set_error_threshold(error_threshold);
          std::dynamic_pointer_cast<gua::node::PLodNode>(geometry_node)->set_radius_scale(radius_scale);
        }
        #endif

        renderer.queue_draw({&graph});
      }
  });

  loop.start();

  //worker thread.join()

  return 0;
}
