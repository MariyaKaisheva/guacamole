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


#include <scm/input/tracking/art_dtrack.h>
#include <scm/input/tracking/target.h>


#include <gua/guacamole.hpp>
#include <gua/utils/Trackball.hpp>
#include <gua/math/math.hpp>
#include <gua/math/BoundingBox.hpp>

#include <gua/node/PLodNode.hpp>
#include <gua/renderer/LodLoader.hpp>
#include <gua/renderer/PBSMaterialFactory.hpp>
#include <gua/renderer/PLodPass.hpp>
#include <gua/renderer/LightVisibilityPass.hpp>
#include <gua/renderer/PLodPass.hpp>
#include <gua/renderer/TexturedQuadPass.hpp>
#include <gua/renderer/DebugViewPass.hpp>
#include <gua/renderer/BBoxPass.hpp>
// Npr-related gua heathers:
#include <gua/renderer/ToonResolvePass.hpp> 
//#include <gua/rendererToonTriMeshRenderer.hpp>
#include <gua/renderer/NprTestPass.hpp>
#include <gua/renderer/NprOutlinePass.hpp>
#include <gua/renderer/NprBlendingPass.hpp>
#include <gua/renderer/NPREffectPass.hpp>

#include <boost/program_options.hpp>

#define USE_SIDE_BY_SIDE 0
#define USE_ANAGLYPH 0
#define USE_MONO 1

#define TRACKING_ENABLED 0
#define USE_LOW_RES_WORKSTATION 0

// global variables
bool close_window = false;
bool reset_scene_position = false;
bool freeze_cut_update = false; //for lod models
bool manipulate_scene_geometry = true;
bool use_ray_pointer = false;
bool show_lense = false;
std::vector<gua::math::BoundingBox<gua::math::vec3> > scene_bounding_boxes;
auto zoom_factor = 1.0;
float screen_width = 0.595f;
float screen_height = 0.3346f;
gua::math::mat4 current_probe_tracking_matrix(gua::math::mat4::identity());
double x_offset_scene_track_target = 0.0;
double y_offset_scene_track_target = 0.0;
double z_offset_scene_track_target = 0.0;

std::set<std::string> model_filenames;

float error_threshold = 3.7f; //for point cloud models
float radius_scale = 0.7f;  //for point cloud models

std::string scenegraph_path = "/model_translation/model_rotation/model_scaling/geometry_root";

// Npr-related global variables ///////////
bool use_toon_resolve_pass = false;
bool apply_halftoning_effect = false;
bool apply_bilateral_filter = false;
bool create_screenspace_outlines = false;
bool no_color = false; 
bool npr_focus = true;
auto surfel_render_mode = gua::PLodPassDescription::SurfelRenderMode::HQ_TWO_PASS;
std::string textrue_file_path = "data/textures/PB2.png";
//---test use-case demo
bool apply_blending = false;
float test_sphere_radius = 0.1f;
bool apply_test_demo = false;
//---
bool toggle_hidden_models = false; 
bool play_kincect_sequence = true; 


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
void take_snapshot (gua::SceneGraph& graph){
  std::cout << graph["/model_translation/model_rotation"]->get_transform() << "ROT \n\n";
  std::cout << graph["/model_translation"]->get_transform() << "TSL \n\n";
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void scale_scene (double zoom_factor, gua::SceneGraph& graph){
  auto scaling_mat = scm::math::make_scale(zoom_factor, zoom_factor, zoom_factor);
  graph["/model_translation/model_rotation/model_scaling"]->set_transform(scaling_mat);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void build_pipe (gua::PipelineDescription& pipe){
  pipe.clear();
  pipe.add_pass(std::make_shared<gua::TriMeshPassDescription>());//tmp
  pipe.add_pass(std::make_shared<gua::PLodPassDescription>());
  pipe.add_pass(std::make_shared<gua::LightVisibilityPassDescription>());
  pipe.add_pass(std::make_shared<gua::ResolvePassDescription>()); 
  //pipe.add_pass(std::make_shared<gua::BBoxPassDescription>());
 //pipe.add_pass(std::make_shared<gua::DebugViewPassDescription>());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void rebuild_pipe(gua::PipelineDescription& pipe) {
  pipe.clear();
  //auto plod_pass = std::make_shared<gua::PLodPassDescription>();
  pipe.add_pass(std::make_shared<gua::TriMeshPassDescription>());//tmp

  plod_pass->mode(surfel_render_mode);
  plod_pass->touch();
  pipe.add_pass(plod_pass);
  pipe.add_pass(std::make_shared<gua::LightVisibilityPassDescription>());

  if(!use_toon_resolve_pass){
    pipe.add_pass(std::make_shared<gua::ResolvePassDescription>());  
  }
  else{
    pipe.add_pass(std::make_shared<gua::ToonResolvePassDescription>());
  }

  if(apply_test_demo){
    pipe.add_pass(std::make_shared<gua::NprTestPassDescription>());
    pipe.get_npr_test_pass()->sphere_radius(test_sphere_radius);
    //std::cout << "Test-demo-pass On with sphere r: " << test_sphere_radius << "\n";
  }
   
  if (apply_bilateral_filter){
    //for(int i = 0; i < num_screenspace_passes; ++i){
      pipe.add_pass(std::make_shared<gua::NPREffectPassDescription>());
    //}
  }
  
  if (create_screenspace_outlines || apply_halftoning_effect){
    pipe.add_pass(std::make_shared<gua::NprOutlinePassDescription>());
    pipe.get_npr_outline_pass()->halftoning(apply_halftoning_effect);
    pipe.get_npr_outline_pass()->apply_outline(create_screenspace_outlines);
    pipe.get_npr_outline_pass()->store_for_blending(false);
    pipe.get_npr_outline_pass()->no_color(no_color);

      if (apply_blending){
        pipe.get_npr_outline_pass()->store_for_blending(true);
        pipe.add_pass(std::make_shared<gua::NprBlendingPassDescription>());
        pipe.get_npr_screen_blending_pass()->focus_appearance(npr_focus);
      }

  }

  //pipe.add_pass(std::make_shared<gua::BBoxPassDescription>());
  //pipe.add_pass(std::make_shared<gua::DebugViewPassDescription>());
  //pipe.add_pass(std::make_shared<gua::SSAAPassDescription>());
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

  // Npr-related options: 
  //surfel rende modes
  if(14 == scancode && action == 1){ // '5'
    surfel_render_mode = gua::PLodPassDescription::SurfelRenderMode::LQ_ONE_PASS;
    std::cout << "LQ_ONE_PASS \n";
    rebuild_pipe(pipe);
  }

  if(15 == scancode && action == 1){ // '6'
    surfel_render_mode = gua::PLodPassDescription::SurfelRenderMode::HQ_TWO_PASS;
    std::cout << "HQ_TWO_PASS \n";
    rebuild_pipe(pipe);
  }

  if(64 == scancode && action == 1){ // 'left Alt'
    npr_focus = !npr_focus;
    rebuild_pipe(pipe);
  }

  //toogle what geometry is manipulated by the tracked cubic probe
  //switches between scene geometry and lense representation
  if(65 == scancode && action == 1){ //'Spacebar'
    manipulate_scene_geometry = !manipulate_scene_geometry;
  }

  //toogle pointer position tracking
  if(108 == scancode && action == 1){ //'right Alt'
    use_ray_pointer = !use_ray_pointer;
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

    case 'p':
      show_lense =! show_lense;
      break;

    case 's':
      //take_snapshot(graph);
      //stop / start player behaviour
      play_kincect_sequence = !play_kincect_sequence; 
      break;

    //Npr-related options: 
    //shading mode
    case 'g':
        if(use_toon_resolve_pass){
          if(pipe.get_toon_resolve_pass()->enable_gooch_shading()) {
            pipe.get_toon_resolve_pass()->enable_gooch_shading(false);
          }
          else {
            pipe.get_toon_resolve_pass()->enable_gooch_shading(true);
          }
        }  
      break;

    //toogle resolve pass
    case 'r':
        use_toon_resolve_pass = !use_toon_resolve_pass;
        rebuild_pipe(pipe);
      break; 

    //bilateral filter - screenspace pass
    case 'b':
        apply_bilateral_filter = !apply_bilateral_filter;
        rebuild_pipe(pipe);
      break; 
    
    //halftoning mode - screenspace pass
    case 'h':
        apply_halftoning_effect = !apply_halftoning_effect;
        rebuild_pipe(pipe);
      break;

    //toogle outlines - screenspace pass
    case 'l':
        create_screenspace_outlines = !create_screenspace_outlines;
        rebuild_pipe(pipe);
      break;

    //toggle color filling
    case 'k':
        no_color = !no_color;
        rebuild_pipe(pipe);
      break;

    //toogle test visualization
    case 't':
        apply_test_demo = !apply_test_demo;
        apply_blending = !apply_blending;


        //create_screenspace_outlines = !create_screenspace_outlines;;
        


        rebuild_pipe(pipe);
      break;
    //decrease blending sphere radius
    case 'n':
        if(test_sphere_radius > 0.005f && apply_test_demo){
          test_sphere_radius -= 0.005f;
          pipe.get_npr_test_pass()->sphere_radius(test_sphere_radius);
          std::cout << "test_sphere_radius: " << test_sphere_radius << "\n"; 
        }
      break;
    //increase blending sphere radius
    case 'm':
        if(test_sphere_radius <= 0.95f && apply_test_demo){
          test_sphere_radius += 0.05f;
          pipe.get_npr_test_pass()->sphere_radius(test_sphere_radius);
        }
      break;

    //reset pipeline
    case 'o': 
        use_toon_resolve_pass = false;
        create_screenspace_outlines = false; 
        apply_test_demo = false;
        apply_bilateral_filter = false; 
        apply_halftoning_effect = false;
        error_threshold = 0.7;
        surfel_render_mode = gua::PLodPassDescription::SurfelRenderMode::HQ_TWO_PASS;
        rebuild_pipe(pipe);
      break; 

    case '7':
        textrue_file_path = "data/textures/black_stroke.png";
      break;

    case '8':
        textrue_file_path = "data/textures/dark_red.png";
      break;

     case '9':
        textrue_file_path = "data/textures/stripes.png";
      break;

     case '0':
        textrue_file_path = "data/textures/important_texture.jpg";
      break;

      case '1':
          toggle_hidden_models = !toggle_hidden_models;
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

  int model_counter = 0;
  for (auto const& model : model_files) {

    std::string node_name = std::to_string(model_counter);
    auto plod_node = lod_loader.load_lod_pointcloud( model );
    plod_node->set_name(node_name);
    if( 0 != model_counter ) {
      plod_node->get_tags().add_tag("invisible");
    }

    graph.add_node(scenegraph_path, plod_node); 
    scene_bounding_boxes.push_back(plod_node->get_bounding_box()); 
    plod_node->set_draw_bounding_box(true);

    //TMP
    model_filenames.insert(model);
    ++model_counter;
  }
   
    graph[scenegraph_path]->update_bounding_box();
    auto scene_bbox = graph[scenegraph_path]->get_bounding_box();
    auto size_along_x = scene_bbox.size(0);
    auto size_along_y = scene_bbox.size(1);
    auto size_along_z = scene_bbox.size(2);
    auto longest_axis = std::max(std::max(size_along_x, size_along_y), std::max(size_along_y, size_along_z));
    float scaling_factor = screen_width / (longest_axis);
    auto all_geometry_nodes = graph[scenegraph_path]->get_children();
    for(auto& node : all_geometry_nodes){
      node->translate(-scene_bbox.center().x, -scene_bbox.center().y, -scene_bbox.center().z);
      node->scale(scaling_factor);
      node->set_draw_bounding_box(true);
    }
    
   graph[scenegraph_path]->set_draw_bounding_box(true);   
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv) {
  // initialize guacamole
  gua::init(1, argv);

	//setup scene ////////////////////////////////////
	gua::SceneGraph graph("main_scenegraph");
  auto model_translation_node = graph.add_node<gua::node::TransformNode>("/", "model_translation");
  auto model_rotation_node = graph.add_node<gua::node::TransformNode>("/model_translation", "model_rotation");
  auto model_scaling_node = graph.add_node<gua::node::TransformNode>("/model_translation/model_rotation", "model_scaling");
	auto geometry_root = graph.add_node<gua::node::TransformNode>("/model_translation/model_rotation/model_scaling", "geometry_root");

  //light source 
	auto light_pointer = graph.add_node<gua::node::TransformNode>("/", "light_pointer");
	auto light_source = graph.add_node<gua::node::LightNode>("/light_pointer", "light_source");
	light_source->data.set_type(gua::node::LightNode::Type::SUN);
	light_source->data.brightness = 3.0f;

  //lense geometry
  auto lense_translation_node = graph.add_node<gua::node::TransformNode>("/", "lense_translation");
  auto lense_rotation_node = graph.add_node<gua::node::TransformNode>("/lense_translation", "lense_rotation");
  auto lense_scaling_node = graph.add_node<gua::node::TransformNode>("/lense_translation/lense_rotation", "lense_scaling");
  auto lense_geometry_root = graph.add_node<gua::node::TransformNode>("/lense_translation/lense_rotation/lense_scaling", "lense_geometry_root");

  //ray
  auto ray_offset = graph.add_node<gua::node::TransformNode>("/", "ray_local_offset");
  auto ray_translation_node = graph.add_node<gua::node::TransformNode>("/ray_local_offset", "ray_translation");
  auto ray_rotation_node = graph.add_node<gua::node::TransformNode>("/ray_local_offset/ray_translation", "ray_rotation");
  

  auto snail_material(gua::MaterialShaderDatabase::instance()
                      ->lookup("gua_default_material")
                      ->make_new_material());

   snail_material->set_uniform("Roughness", 0.0f);
   snail_material->set_uniform("Metalness", 0.0f);

  std::string directory("data/textures/");
  snail_material->set_uniform("ColorMap", directory + "snail_color.png");
  snail_material->set_uniform("Emissivity", 1.0f);
  //snail_material->set_show_back_faces(false);

  gua::TriMeshLoader   trimesh_loader;
  auto ring_geometry(trimesh_loader.create_geometry_from_file(
                "ring", "data/objects/wired_icosphere.obj", // "data/objects/dashed_ring_1b025.obj", 
                gua::TriMeshLoader::NORMALIZE_POSITION |
                gua::TriMeshLoader::NORMALIZE_SCALE )); 

  auto sphere(trimesh_loader.create_geometry_from_file(
                "icosphere", "data/objects/icosphere.obj", 
                gua::TriMeshLoader::NORMALIZE_POSITION |
                gua::TriMeshLoader::NORMALIZE_SCALE |
                gua::TriMeshLoader::LOAD_MATERIALS));

  auto ray_geometry(trimesh_loader.create_geometry_from_file(
                      "ray", "data/objects/ray_cylinder.obj",
                      gua::TriMeshLoader::NORMALIZE_SCALE |
                      gua::TriMeshLoader::LOAD_MATERIALS));

  ///////TMP
  //---snail----
  auto snail(trimesh_loader.create_geometry_from_file(
     "snail", "data/objects/ohluvka.obj",
      snail_material,
      gua::TriMeshLoader::NORMALIZE_POSITION |
          gua::TriMeshLoader::NORMALIZE_SCALE));
  snail->scale(0.4, 0.4, 0.4);
  ///////TMP

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
  auto initial_scene_rotation_mat = gua::math::mat4(scm::math::make_rotation(-45.0, 1.0, 0.0, 0.0)); 
  auto initial_scene_scaling_mat = gua::math::mat4(scm::math::make_scale(1.0, 1.0, 1.0));
  #else
  auto initial_scene_translation_mat = gua::math::mat4(scm::math::make_translation(0.0, 0.0, -0.18));
  auto initial_scene_rotation_mat = gua::math::mat4(scm::math::make_rotation(-45.0, 1.0, 0.0, 0.0)); 
  auto initial_scene_scaling_mat = gua::math::mat4(scm::math::make_scale(0.15, 0.15, 0.15));
  #endif

  
  model_translation_node->set_transform(initial_scene_translation_mat);
  model_rotation_node->set_transform(initial_scene_rotation_mat);
  model_scaling_node->set_transform(initial_scene_scaling_mat);

  lense_translation_node->set_transform(initial_scene_translation_mat);
  lense_rotation_node->set_transform(initial_scene_rotation_mat);
  //lense_scaling_node->set_transform(initial_scene_scaling_mat);

  ray_translation_node->set_transform(initial_scene_translation_mat);
  ray_rotation_node->set_transform(initial_scene_rotation_mat);
  ray_offset->set_transform(scm::math::make_translation(0.0, 0.0, 0.0));

  //graph.add_node(lense_geometry_root, sphere);
  graph.add_node(lense_geometry_root, ring_geometry);
  //graph.add_node(lense_geometry_root, snail); //TMP

  graph.add_node(ray_rotation_node, ray_geometry);

  //add pipeline with rendering passes
  auto pipe = std::make_shared<gua::PipelineDescription>();
  rebuild_pipe(*pipe);
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
    gua::math::mat4 current_ray_tracking_matrix(gua::math::mat4::identity());

	  std::thread tracking_thread([&]() {
      scm::inp::tracker::target_container targets;
      targets.insert(scm::inp::tracker::target_container::value_type(5, scm::inp::target(5)));
      targets.insert(scm::inp::tracker::target_container::value_type(22, scm::inp::target(22)));
      targets.insert(scm::inp::tracker::target_container::value_type(17, scm::inp::target(17)));

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

        auto ray_origin_target = targets.find(17)->second.transform();
        ray_origin_target[12] /= 1000.f; 
        ray_origin_target[13] /= 1000.f; 
        ray_origin_target[14] /= 1000.f;
        current_ray_tracking_matrix = ray_origin_target;
      }
    });
	#endif

  //tmp
  //loading texutes 
  gua::TextureDatabase::instance()->load(textrue_file_path);
  gua::TextureDatabase::instance()->load("data/textures/black_stroke.png");
  gua::TextureDatabase::instance()->load("data/textures/dark_red.png");
  gua::TextureDatabase::instance()->load("data/textures/stripes.png");
  gua::TextureDatabase::instance()->load("data/textures/important_texture.jpg");

  auto initial_translation_in_tracking_space_coordinates = gua::math::mat4::identity();
  auto initial_rotation_in_tracking_space_coordinates = gua::math::mat4::identity();
  auto ray_forward = gua::math::vec3(1.0f, 0.0f, 0.0f); 
  gua::LodLoader lod_loader;
	//application loop //////////////////////////////
	gua::events::MainLoop loop;
	double tick_time = 1.0/500.0;
	gua::events::Ticker ticker(loop, tick_time);
  size_t ctr{};
	ticker.on_tick.connect([&](){

		//terminate application
		if (window->should_close() || close_window) {
			renderer.stop();
    		window->close();
    		loop.stop();
    	}
    	else {
        auto tex = gua::TextureDatabase::instance()->lookup(textrue_file_path);
        #if(USE_SIDE_BY_SIDE && TRACKING_ENABLED)
          /*#if 1
            camera->set_transform(current_cam_tracking_matrix);
          #endif */

          auto current_probe_rotation_mat = gua::math::get_rotation(current_probe_tracking_matrix);
          auto current_probe_translation_mat = scm::math::make_translation(current_probe_tracking_matrix[12], current_probe_tracking_matrix[13], current_probe_tracking_matrix[14]);
          auto ray_translation_mat = scm::math::make_translation(current_ray_tracking_matrix[12], 
                                                                   current_ray_tracking_matrix[13], 
                                                                   current_ray_tracking_matrix[14]);
          auto ray_rotation_mat = gua::math::get_rotation(current_ray_tracking_matrix);
          if(!trackball.get_left_button_press_state()){   
            if(manipulate_scene_geometry)  {
              initial_rotation_in_tracking_space_coordinates = scm::math::inverse(current_probe_rotation_mat)*(model_rotation_node->get_transform());  
              initial_translation_in_tracking_space_coordinates = scm::math::inverse(current_probe_translation_mat)*(model_translation_node->get_transform());
           }
           else{
              initial_rotation_in_tracking_space_coordinates = scm::math::inverse(current_probe_rotation_mat)*(lense_rotation_node->get_transform());  
              initial_translation_in_tracking_space_coordinates = scm::math::inverse(current_probe_translation_mat)*(lense_translation_node->get_transform());
           }       

          }
          else{
            auto rotation_mat = current_probe_rotation_mat*initial_rotation_in_tracking_space_coordinates;
            auto translation_mat = current_probe_translation_mat*initial_translation_in_tracking_space_coordinates;
            if(manipulate_scene_geometry){
              model_translation_node->set_transform(translation_mat);
              model_rotation_node->set_transform(rotation_mat); 
            }
            else if(!use_ray_pointer){
              lense_translation_node->set_transform(translation_mat);
              lense_rotation_node->set_transform(rotation_mat);
            }
          }

          if(reset_scene_position){
            if(manipulate_scene_geometry){
              model_translation_node->set_transform(initial_scene_translation_mat);
              model_rotation_node->set_transform(initial_scene_rotation_mat);
              scale_scene(1.0, graph);
              zoom_factor = 1.0;
            }
            else{
              lense_translation_node->set_transform(initial_scene_translation_mat);
              lense_rotation_node->set_transform(initial_scene_rotation_mat);
            }
            reset_scene_position = false;
          } 

          double scale_factor = test_sphere_radius*1.5; //75% influence radius 
          lense_scaling_node->set_transform(scm::math::make_scale(scale_factor, scale_factor, scale_factor));

          if(!use_ray_pointer){
            if(apply_test_demo){
              gua::math::vec3d translation_vec = gua::math::get_translation(lense_translation_node->get_transform());
              pipe->get_npr_test_pass()->sphere_location((gua::math::vec3f)translation_vec);
              pipe->get_npr_screen_blending_pass()->focus_appearance(npr_focus);
            }
          }
          else{
            ray_translation_node->set_transform(ray_translation_mat);
            ray_rotation_node->set_transform(ray_rotation_mat);
            gua::math::vec3 current_ray_forward = gua::math::vec3(ray_rotation_mat*gua::math::vec4(ray_forward));
            auto intersection = lod_loader.pick_lod_bvh(ray_geometry->get_world_position(),
                                                        current_ray_forward,
                                                        3.0f,
                                                        model_filenames,
                                                        1.0f);
            lense_translation_node->set_transform(scm::math::make_translation(intersection.second.x, 
                                                                              intersection.second.y, 
                                                                              intersection.second.z));
            if(apply_test_demo){
              pipe->get_npr_test_pass()->sphere_location((gua::math::vec3f)intersection.second);
            }
          }

          

          uint child_counter = 0;
          for(auto& geometry_node : geometry_root->get_children()){
            std::dynamic_pointer_cast<gua::node::PLodNode>(geometry_node)->set_cut_dispatch(freeze_cut_update);
            std::dynamic_pointer_cast<gua::node::PLodNode>(geometry_node)->set_error_threshold(error_threshold);

            if( 0 != child_counter) {
              std::dynamic_pointer_cast<gua::node::PLodNode>(geometry_node)->set_radius_scale(radius_scale);
            }
            std::dynamic_pointer_cast<gua::node::PLodNode>(geometry_node)->set_texture(tex);

            ++child_counter;
          }

          if(use_ray_pointer==false){
            graph["/ray_local_offset"]->get_tags().add_tag("invisible");
          }
          else{
            graph["/ray_local_offset"]->get_tags().remove_tag("invisible");
          }

          if(show_lense==false){
            graph["/lense_translation/lense_rotation/lense_scaling/lense_geometry_root"]->get_tags().add_tag("invisible");
          }
          else{
            graph["/lense_translation/lense_rotation/lense_scaling/lense_geometry_root"]->get_tags().remove_tag("invisible");
          }

          if(toggle_hidden_models){
            for(int i = 0; i < geometry_root->get_children().size(); ++i) {
              auto& current_child = geometry_root->get_children().at(i);
              if(current_child->get_tags().has_tag("invisible")){
                current_child->get_tags().remove_tag("invisible");
              } else {
                current_child->get_tags().add_tag("invisible");
              }
            }
            toggle_hidden_models = false;
          }
        #else
              // apply trackball matrix to object
        gua::math::mat4 modelmatrix = scm::math::make_translation(gua::math::float_t(trackball.shiftx()),
                                                                  gua::math::float_t(trackball.shifty()),
                                                                  gua::math::float_t(trackball.distance())) *
                                      gua::math::mat4(trackball.rotation());

          model_translation_node->set_transform(modelmatrix);
         graph["/ray_local_offset"]->get_tags().add_tag("invisible");
         graph["/lense_translation/lense_rotation/lense_scaling/lense_geometry_root"]->get_tags().add_tag("invisible");
          uint child_counter = 0;
          for(auto& geometry_node : geometry_root->get_children()){
            std::dynamic_pointer_cast<gua::node::PLodNode>(geometry_node)->set_cut_dispatch(freeze_cut_update);
            std::dynamic_pointer_cast<gua::node::PLodNode>(geometry_node)->set_error_threshold(error_threshold);

           // if( 0 != child_counter) {
              std::dynamic_pointer_cast<gua::node::PLodNode>(geometry_node)->set_radius_scale(radius_scale);
            //}
            std::dynamic_pointer_cast<gua::node::PLodNode>(geometry_node)->set_texture(tex);

            ++child_counter;
          }
          if(toggle_hidden_models){
              for(int i = 0; i < geometry_root->get_children().size(); ++i) {
                auto& current_child = geometry_root->get_children().at(i);
                if(current_child->get_tags().has_tag("invisible")){
                  current_child->get_tags().remove_tag("invisible");
                } else {
                  current_child->get_tags().add_tag("invisible");
                }
                //std::cout <<  geometry_root->get_children().at(i)->get_name() << std::endl;
              }
              toggle_hidden_models = false;
          }
        #endif

         /*if (ctr++ % 150 == 0){
          std::cout << "Frame time: " << 1000.f / window->get_rendering_fps() << " ms, fps: "
                << window->get_rendering_fps() << ", app fps: "
                << renderer.get_application_fps() << std::endl;
         }*/
          
    		renderer.queue_draw({&graph});
    	}
	});

	loop.start();

	return 0;
}
