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
#include <gua/renderer/NprTestPass.hpp>
#include <gua/renderer/NprOutlinePass.hpp>
#include <gua/renderer/NprBlendingPass.hpp>
#include <gua/renderer/NPREffectPass.hpp>

#include <boost/program_options.hpp>

#define USE_SIDE_BY_SIDE 1
#define USE_ANAGLYPH 0
#define USE_MONO 0

#define TRACKING_ENABLED 1
#define USE_LOW_RES_WORKSTATION 0

// global variables
bool print_mat = false; //tmp
bool close_window = false;
bool reset_scene_position = false;
bool detach_scene_from_tracking_target = false;
bool force_detach_on_key_press = false;
bool offset_adjustment_on_click = false;
bool use_ray_pointer = false;
std::vector<gua::math::BoundingBox<gua::math::vec3> > scene_bounding_boxes;
auto zoom_factor = 1.0;
float screen_width = 0.595f;
float screen_height = 0.3346f;
gua::math::mat4 current_probe_tracking_matrix(gua::math::mat4::identity());
double x_offset_scene_track_target = 0.0;
double y_offset_scene_track_target = 0.0;
double z_offset_scene_track_target = 0.0;

float error_threshold = 3.7f; //for point cloud models

std::string scenegraph_path = "/scene_root";

// Npr-related global variables ///////////
bool use_toon_resolve_pass = false;
bool apply_halftoning_effect = false;
bool apply_bilateral_filter = false;
bool create_screenspace_outlines = false;
bool no_color = false; 
auto surfel_render_mode = gua::PLodPassDescription::SurfelRenderMode::HQ_TWO_PASS;
std::string textrue_file_path = "data/textures/dark_red.png";
//---test use-case demo
bool apply_blending = false;
float test_sphere_radius = 0.1f;
bool apply_test_demo = false;
//---

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
void change_scene_probe_offset(scm::math::vec3d& target_translation_vector, scm::math::vec3d&  old_scene_position){
  x_offset_scene_track_target = std::fabs(target_translation_vector.x - old_scene_position.x);
  y_offset_scene_track_target = std::fabs(target_translation_vector.y - old_scene_position.y);
  z_offset_scene_track_target = std::fabs(target_translation_vector.z - old_scene_position.z);

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void scale_scene (double zoom_factor, gua::SceneGraph& graph){
  auto curent_scene_transform_mat = graph[scenegraph_path]->get_world_transform();
  gua::math::vec3 current_scale_factor_vec = gua::math::vec3(gua::math::get_scale(curent_scene_transform_mat));
  auto scale_correction_mat = scm::math::make_scale(1.0/current_scale_factor_vec.x, 1.0/current_scale_factor_vec.x, 1.0/current_scale_factor_vec.x);
  auto scene_scaling_mat = scale_correction_mat*scm::math::make_scale(zoom_factor, zoom_factor, zoom_factor);
  auto scene_transform_mat = curent_scene_transform_mat*scene_scaling_mat;

  graph[scenegraph_path]->set_transform(scene_transform_mat);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void build_pipe (gua::PipelineDescription& pipe){
  pipe.clear();
  pipe.add_pass(std::make_shared<gua::TriMeshPassDescription>());//tmp
  pipe.add_pass(std::make_shared<gua::PLodPassDescription>());
  pipe.add_pass(std::make_shared<gua::LightVisibilityPassDescription>());
  pipe.add_pass(std::make_shared<gua::ResolvePassDescription>()); 
  pipe.add_pass(std::make_shared<gua::BBoxPassDescription>());
 // pipe.add_pass(std::make_shared<gua::DebugViewPassDescription>());
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
    std::cout << "Test-demo-pass On with sphere r: " << test_sphere_radius << "\n";
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
    }
  }
  pipe.add_pass(std::make_shared<gua::BBoxPassDescription>());
  pipe.add_pass(std::make_shared<gua::DebugViewPassDescription>());
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

  //toogle scene position tracking
  if(65 == scancode && action == 1){ //'Spacebar'
      force_detach_on_key_press = !force_detach_on_key_press;
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

    case 'z':
        if(error_threshold <= 400.0){
          error_threshold += 2.0;
        }
      break;

    case 'x':
        if(error_threshold >= 0.2){
          error_threshold -= 0.19;
        }
      break;

    case 'p':
      print_mat = !print_mat;
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

    //bilateral filter - screenspace passcreate_screenspace_outlines =
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
        rebuild_pipe(pipe);
      break;  
    
    default:
      break;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::pair<std::vector<std::string>, std::vector<scm::math::mat4f>> interpret_config_file(std::string const&  path_to_file) {
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
  /*auto colorful_material(gua::MaterialShaderDatabase::instance()
                          ->lookup("gua_default_material")
                          ->make_new_material());
  colorful_material->set_uniform("ColorMap", textrue_file_path);*/

       //tmp texture for point cloud npr test
    gua::TextureDatabase::instance()->load(textrue_file_path);
    auto tex = gua::TextureDatabase::instance()->lookup(textrue_file_path);

  //int model_counter = 0;
  for (auto& model : model_files) {

    //auto plod_node = lod_loader.load_lod_pointcloud("plod_node_" + std::to_string(model_counter), model, colorful_material);
    auto plod_node = lod_loader.load_lod_pointcloud(model);
    plod_node->set_texture(tex);
    //model_counter += 1;
    //plod_node->get_material()->set_uniform("ColorMap", std::string("data/textures/colored_grid.png"));
     // auto lod_keep_input_desc = std::make_shared<gua::MaterialShaderDescription>("./data/materials/PLOD_use_input_color.gmd");
  //auto lod_keep_color_shader(std::make_shared<gua::MaterialShader>("PLOD_pass_input_color", lod_keep_input_desc));
  //gua::MaterialShaderDatabase::instance()->add(lod_keep_color_shader);
 //   auto plod_material = plod_node->get_material();
  //  plod_material->set_uniform("ColorMap", std::string("data/textures/colored_grid.png"));
  //  plod_node->set_material(plod_material);
    graph.add_node(scenegraph_path, plod_node); 
    scene_bounding_boxes.push_back(plod_node->get_bounding_box()); 
    plod_node->set_draw_bounding_box(true);
  }
   //std::dynamic_pointer_cast<gua::node::PLodNode>(node)->get_material()->set_uniform("ColorMap", std::string("data/textures/colored_grid.png"));
    graph[scenegraph_path]->update_bounding_box();
    auto scene_bbox = graph[scenegraph_path]->get_bounding_box();
    auto size_along_x = scene_bbox.size(0);
    auto size_along_y = scene_bbox.size(1);
    auto size_along_z = scene_bbox.size(2);
    std::cout << size_along_x <<" x " << size_along_y <<" y " << size_along_z <<" z " << "SF\n";
    auto longest_axis = std::max(std::max(size_along_x, size_along_y), std::max(size_along_y, size_along_z));
    
    float scaling_factor = screen_width / (longest_axis*4.0);
    std::cout << scaling_factor << "SF\n";
    auto all_geometry_nodes = graph[scenegraph_path]->get_children();
    for(auto& node : all_geometry_nodes){
      node->translate(-scene_bbox.center().x, -scene_bbox.center().y, -scene_bbox.center().z);
      node->translate(0.0, 1.6, 0.0);
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
	auto scene_transform = graph.add_node<gua::node::TransformNode>("/", "scene_root");
  //auto scene_translation_node = graph.add_node<gua::node::TransformNode>("/", "translation_node");
  //auto scene_rotation_node = graph.add_node<gua::node::TransformNode>("/translation_node", "rotation_node");
  auto initial_scene_translation_vec = scm::math::vec<double, 3u>(0.0, y_offset_scene_track_target, 0.0);
  //scene_transform->translate(initial_scene_translation_vec);
  //scene_transform->translate(initial_scene_translation_vec.x, initial_scene_translation_vec.y, initial_scene_translation_vec.z);
	auto light_pointer = graph.add_node<gua::node::TransformNode>("/", "light_pointer");
	auto light_source = graph.add_node<gua::node::LightNode>("/light_pointer", "light_source");
	light_source->data.set_type(gua::node::LightNode::Type::SUN);
	light_source->data.brightness = 3.0f;

  //TMP try loading sample models to see if window gets created
  //create simple untextured material shader
  /*auto lod_keep_input_desc = std::make_shared<gua::MaterialShaderDescription>("./data/materials/PLOD_use_input_color.gmd");
  auto lod_keep_color_shader(std::make_shared<gua::MaterialShader>("PLOD_pass_input_color", lod_keep_input_desc));
  gua::MaterialShaderDatabase::instance()->add(lod_keep_color_shader);
  auto lod_rough = lod_keep_color_shader->make_new_material();
  lod_rough->set_uniform("metalness", 0.5f);
  lod_rough->set_uniform("roughness", 1.0f);
  lod_rough->set_uniform("emissivity", 0.0f);
  gua::LodLoader lod_loader;
  lod_loader.set_out_of_core_budget_in_mb(8000);
  lod_loader.set_render_budget_in_mb(6000);
  lod_loader.set_upload_budget_in_mb(32);*/
  /*auto plod_head = lod_loader.load_lod_pointcloud("pointcloud_head",
                                                   "/mnt/pitoti/hallermann_scans/bruecke2/0_kopf_local.bvh",
                                                   lod_rough,
                                                   gua::LodLoader::NORMALIZE_POSITION |
                                                   gua::LodLoader::NORMALIZE_SCALE);*/




  auto snail_material(gua::MaterialShaderDatabase::instance()
                      ->lookup("gua_default_material")
                      ->make_new_material());

   snail_material->set_uniform("Roughness", 0.0f);
   snail_material->set_uniform("Metalness", 0.0f);

  std::string directory("data/textures/");
  snail_material->set_uniform("ColorMap", directory + "blue_marks.png");
  snail_material->set_uniform("Emissivity", 1.0f);
  snail_material->set_show_back_faces(true);

  gua::TriMeshLoader   trimesh_loader;
  auto ring_geometry(trimesh_loader.create_geometry_from_file(
                "icosphere", "data/objects/dashed_ring_1b025.obj", 
                snail_material,
                gua::TriMeshLoader::NORMALIZE_POSITION |
                gua::TriMeshLoader::NORMALIZE_SCALE )); 

  auto sphere(trimesh_loader.create_geometry_from_file(
                "icosphere", "data/objects/icosphere.obj", 
                gua::TriMeshLoader::NORMALIZE_POSITION |
                gua::TriMeshLoader::NORMALIZE_SCALE |
                gua::TriMeshLoader::LOAD_MATERIALS));

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
  #else
  screen->translate(0, 0, -0.6);
  #endif

	//camera configuration ///////////////////////////
  #if USE_SIDE_BY_SIDE 
  auto navigation_eye_offset = graph.add_node<gua::node::TransformNode>("/", "eye_offset");
  auto camera = graph.add_node<gua::node::CameraNode>("/eye_offset", "cam");
  camera->translate(screen_center_offset_x - distance_to_screen, screen_center_offset_y, screen_center_offset_z);
  navigation_eye_offset->translate(0, 0, glass_eye_offset);
  #else 
  auto camera = graph.add_node<gua::node::CameraNode>("/screen", "cam");
  camera->translate(0.0, 0.0, - distance_to_screen);
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

   //////////////////input handling addapted from Lamure /////////////
  namespace po = boost::program_options;
 
  std::string resource_file = "auto_generated.rsc";
  po::options_description desc("options: ");
  desc.add_options()
    ("help,h", "print help message")
    ("input,f", po::value<std::string>(), "specify input file with scene configuration")
    ("models,m", po::value <std::vector<std::string>>()->multitoken(), "list paths to desired input models")
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
  auto initial_scene_translation_mat = gua::math::mat4(scm::math::make_translation(screen_center_offset_x, 
                                                                                  screen_center_offset_y, 
                                                                                  screen_center_offset_z));
  auto initial_scene_rotation_mat = gua::math::mat4(scm::math::make_rotation(-45.0, 1.0, 0.0, 0.0)); 
  auto initial_scene_scaling_mat = gua::math::mat4(scm::math::make_scale(1.0, 1.0, 1.0));
  auto initial_scene_transform_mat = initial_scene_translation_mat*initial_scene_rotation_mat*initial_scene_scaling_mat;
  
  graph[scenegraph_path]->set_transform(initial_scene_transform_mat);


  //TODO: clean up the code from unused RayNode
  //ray_test
  auto ray_node = graph.add_node<gua::node::RayNode>("/", "ray_node");
  //TMP -- user geometry to signal the center of the tracking target
  //sphere->scale(0.0005);
  graph.add_node(ray_node, sphere);
  graph.add_node(ray_node, ring_geometry);

  //add pipeline with rendering passes
  auto pipe = std::make_shared<gua::PipelineDescription>();
  rebuild_pipe(*pipe);
  camera->set_pipeline_description(pipe);

	//add mouse interaction
	gua::utils::Trackball trackball(0.1, 0.02, 0.02); 

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
    gua::math::mat4 current_pointer_tracking_matrix(gua::math::mat4::identity());

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

       //if(!detach_scene_from_tracking_target || offset_adjustment_on_click){
          auto scene_root_target = targets.find(22)->second.transform();
          scene_root_target[12] /= 1000.f; 
          scene_root_target[13] /= 1000.f;
          scene_root_target[14] /= 1000.f;
          current_probe_tracking_matrix = scene_root_target;
        //}

        if(use_ray_pointer) {
          auto ray_origin_target = targets.find(17)->second.transform();
          ray_origin_target[12] /= 1000.f; 
          ray_origin_target[13] /= 1000.f; 
          ray_origin_target[14] /= 1000.f;
          current_pointer_tracking_matrix = ray_origin_target;
        }
      }
    });
	#endif

  auto scene_transform_mat = gua::math::mat4::identity();
  //auto scene_rotation_mat = gua::math::mat4::identity();
  //auto scene_scaling_mat = gua::math::mat4::identity();
  //auto scene_translation_mat = gua::math::mat4::identity();
  //auto initial_scene_translation_mat_in_probe_coordinates = gua::math::mat4::identity(); 
  //auto initial_scene_rotation_mat_in_probe_coordinates =  gua::math::mat4::identity();
  //auto initial_scene_scaling_mat_in_probe_coordinates =  gua::math::mat4::identity();
  auto initial_scene_transform_mat_in_probe_coordinates = gua::math::mat4::identity();
  /*auto current_probe_translation_mat = scm::math::make_translation(current_probe_tracking_matrix[12], current_probe_tracking_matrix[13], current_probe_tracking_matrix[14]);
  auto initial_scene_translation_mat_in_probe_coordinates = scm::math::inverse(current_probe_translation_mat)*initial_scene_translation_mat;
  auto initial_scene_rotation_mat_in_probe_coordinates = scm::math::inverse(gua::math::get_rotation(current_probe_tracking_matrix))*initial_scene_rotation_mat;*/

	//application loop //////////////////////////////
	gua::events::MainLoop loop;
	double tick_time = 1.0/500.0;
	gua::events::Ticker ticker(loop, tick_time);
	ticker.on_tick.connect([&](){

		//terminate application
		if (window->should_close() || close_window) {
			renderer.stop();
    		window->close();
    		loop.stop();
    	}
    	else {
        #if(USE_SIDE_BY_SIDE && TRACKING_ENABLED)
          #if 0
            camera->set_transform(current_cam_tracking_matrix);
          #endif 

          if(!trackball.get_left_button_press_state()){ //no dragging; static scene and free moving rpobe 
            detach_scene_from_tracking_target = true;
            initial_scene_transform_mat_in_probe_coordinates = scm::math::inverse(current_probe_tracking_matrix)
                                                               * graph[scenegraph_path]->get_world_transform(); //initial_scene_transform_mat;
          }
          else{//dragging; express scene transformations in coordinatre system of the probe local coordinates 
            scene_transform_mat = current_probe_tracking_matrix 
                                  * initial_scene_transform_mat_in_probe_coordinates;
            detach_scene_from_tracking_target = false;
            initial_scene_transform_mat = scene_transform_mat;
          }

          if(reset_scene_position){
            //gua::math::vec3 scale_vec = gua::math::vec3(gua::math::get_scale(scene_transform->get_world_transform()));
            gua::math::vec3 scale_vec = gua::math::vec3(gua::math::get_scale(initial_scene_transform_mat));
            //auto scene_unscaling_mat = scm::math::make_scale(1.0/scale_vec.x, 1.0/scale_vec.y, 1.0/scale_vec.z);
            auto scene_translation_mat = gua::math::mat4(scm::math::make_translation(screen_center_offset_x, 
                                                                                  screen_center_offset_y, 
                                                                                  screen_center_offset_z));
            auto scene_rotation_mat = gua::math::mat4(scm::math::make_rotation(-45.0, 1.0, 0.0, 0.0)); 
            scene_transform_mat = scene_translation_mat
                                  * scene_rotation_mat;
                                  //* scene_unscaling_mat;
            scene_transform->set_transform(scene_transform_mat); 
            std::cout<< "zoom_factor" << zoom_factor << "\n";                     
            zoom_factor = 1.0;
            scale_scene(1.0, graph);
            std::cout<< "scale_vec" << scale_vec << "\n";
            reset_scene_position = false;
          }

          if(!detach_scene_from_tracking_target){
            scene_transform->set_transform(scene_transform_mat); 
          }  

          if(print_mat){
            std::cout << "\n";
          }

          if(use_ray_pointer){
            double scale_factor = test_sphere_radius*0.5;
            ray_node->set_transform(current_pointer_tracking_matrix*scm::math::make_scale(scale_factor, scale_factor, scale_factor));
            //auto intersection = ray_node->intersect(scene_transform->get_bounding_box()); //TODO take var initialization out of the application loop
            if(apply_test_demo){
              pipe->get_npr_test_pass()->sphere_location((gua::math::vec3f)gua::math::get_translation(current_pointer_tracking_matrix));
            }
          }

          for(auto& scene_node : scene_transform->get_children()){
            std::dynamic_pointer_cast<gua::node::PLodNode>(scene_node)->set_error_threshold(error_threshold);
          }
          /*for(auto& scene_node : scene_rotation_node->get_children()){
            std::dynamic_pointer_cast<gua::node::PLodNode>(scene_node)->set_error_threshold(error_threshold);
          }*/
          

        #endif
    		renderer.queue_draw({&graph});
    	}
	});

	loop.start();

	return 0;
}