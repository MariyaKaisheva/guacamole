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

#include <boost/program_options.hpp>

#define USE_SIDE_BY_SIDE 1
#define USE_ANAGLYPH 0
#define USE_MONO 0

#define TRACKING_ENABLED 1
#define USE_LOW_RES_WORKSTATION 0

//global variables
bool close_window = false;
bool detach_scene_from_tracking_target = false;
std::vector<gua::math::BoundingBox<gua::math::vec3> > scene_bounding_boxes;
auto zoom_factor = 1.0;
gua::math::mat4 current_scene_tracking_matrix(gua::math::mat4::identity());
double x_offset_scene_track_target = 0.0;
double y_offset_scene_track_target = 0.1;

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

void scale_scene (double zoom_factor, gua::SceneGraph& graph){
  /*
  //auto translation =  scm::math::make_translation(graph["scene_root"]->get_world_position());
  auto translation = scm::math::make_translation(gua::math::get_translation(graph["scene_root"]->get_transform()) );
  auto rotation = gua::math::get_rotation(graph["scene_root"]->get_transform());
  auto scale = scm::math::make_scale(zoom_factor, zoom_factor, zoom_factor);
  auto scene_transform_mat =  translation * rotation * scale;
  auto transform = graph["scene_root"]->get_world_transform();
  //std::cout << scene_transform_mat << "STM \n";
  std::cout << translation << "TM \n";
  std::cout << rotation << "RM \n";
  //graph["scene_root"]->scale(zoom_factor);
  graph["scene_root"]->set_transform(scene_transform_mat);
  */

  
  auto current_scene_translation_vec = gua::math::get_translation(current_scene_tracking_matrix);
  auto current_scene_rot_matrix = gua::math::get_rotation(current_scene_tracking_matrix);


  auto scene_transform_mat = scm::math::make_translation(current_scene_translation_vec)
                             * current_scene_rot_matrix
                             * scm::math::make_scale(zoom_factor, zoom_factor, zoom_factor);                         
                             //* scm::math::make_scale(scaling_factor, scaling_factor, scaling_factor);
                             //scene_transform->get_transform();// *
                             //scm::math::make_translation(-scene_bbox.center().x, -scene_bbox.center().y, -scene_bbox.center().z)*
                             //current_scene_rot_matrix;
  graph["/scene_root"]->set_transform(scene_transform_mat);


}

//key-board interactions
void key_press(gua::PipelineDescription& pipe, 
			   gua::SceneGraph& graph, 
			   int key,
			   int scancode, 
			   int action, 
			   int mods){
  //TODO what action mapps to what???
  //if (action == 0) return;
  if(9 == scancode){ //'Esc'
      close_window = true;
  }

  //toogle scene position tracking
  if(65 == scancode && action == 1){ //'Spacebar'
      detach_scene_from_tracking_target = !detach_scene_from_tracking_target;
  }
  
	switch(std::tolower(key)){
		case 't':
		  std::cout << "t key was pressed \n";
		  break;

    case '-':
      if(zoom_factor > 0.05){
        zoom_factor -= 0.05;
      }
      scale_scene(zoom_factor, graph);
      break;

    case '=':
        zoom_factor += 0.05;
        scale_scene(zoom_factor, graph);
      break;

    default:
      break;
	}
}

void build_pipe (gua::PipelineDescription& pipe){
	pipe.clear();
  pipe.add_pass(std::make_shared<gua::PLodPassDescription>());
	pipe.add_pass(std::make_shared<gua::LightVisibilityPassDescription>());
  pipe.add_pass(std::make_shared<gua::ResolvePassDescription>()); 
  pipe.add_pass(std::make_shared<gua::BBoxPassDescription>());
 // pipe.add_pass(std::make_shared<gua::DebugViewPassDescription>());
}

std::pair<std::vector<std::string>, std::vector<scm::math::mat4f>> interpret_config_file(std::string const&  path_to_file) {
  namespace fs = boost::filesystem;
  //std::cout << "Do stng with scene config file "<< path_to_file << "\n";
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


/*std::pair<double, scm::math::vec<double, 3u>>
compute_normalizing_transformations(std::vector<gua::math::BoundingBox<gua::math::vec3> > const& bounding_boxes){
  //TODO check if next 2 lines make sense???
  auto total_max_point = std::numeric_limits<scm::math::vec<double, 3u>>::lowest();
  auto total_min_point = std::numeric_limits<scm::math::vec<double, 3u>>::max();
  for(auto& current_bbox : bounding_boxes){

   total_max_point.x = std::max(total_max_point.x, current_bbox.max.x);
   total_max_point.y = std::max(total_max_point.y, current_bbox.max.y);
   total_max_point.z = std::max(total_max_point.z, current_bbox.max.z);
   total_min_point.x = std::min(total_min_point.x, current_bbox.min.x);
   total_min_point.y = std::min(total_min_point.y, current_bbox.min.y);
   total_min_point.z = std::min(total_min_point.z, current_bbox.min.z);
  }
  std::cout << "total_max_point: " << total_max_point << "\n";
  std::cout << "total_min_point: " << total_min_point << "\n";
  auto bbox_center_point = (total_max_point + total_min_point)/2.0;
  auto scale = 0.59f / scm::math::length(total_max_point - total_min_point);
  std::cout << scale << " Scale \n";
  return std::make_pair(scale, bbox_center_point);
}*/

void add_models_to_graph(std::vector<std::string> const& model_files, 
                         gua::SceneGraph& graph,
                         std::vector<scm::math::mat4f> const& model_transformations){
  gua::LodLoader lod_loader;
  for (auto& model : model_files) {
    auto plod_node = lod_loader.load_lod_pointcloud(model);
    graph.add_node("/scene_root", plod_node); 
    scene_bounding_boxes.push_back(plod_node->get_bounding_box()); 
    plod_node->set_draw_bounding_box(true);
  }
    //auto normalization_values = compute_normalizing_transformations(scene_bounding_boxes);
   
    graph["/scene_root"]->update_bounding_box();
    auto scene_bbox = graph["/scene_root"]->get_bounding_box();
   // auto total_max_point = scene_bbox.max;
   // auto total_min_point = scene_bbox.min;
    //TODO: should use screen width as fration numerator??? 
    //this number represents the desired length of longest diagonal of bbox 
    float screen_width = 0.595f;
    float screen_height = 0.3346f;
    float screen_diagonal = std::sqrt(screen_width*screen_width + screen_height*screen_height);
    float screen_volume_depth = 0.2;
    float scaling_fraction_numerator =  std::sqrt(screen_diagonal*screen_diagonal + screen_volume_depth*screen_volume_depth);

    auto size_along_x = scene_bbox.size(0);
    auto size_along_y = scene_bbox.size(1);
    auto size_along_z = scene_bbox.size(2);
    std::cout << size_along_x <<" x " << size_along_y <<" y " << size_along_z <<" z " << "SF\n";
    auto longest_axis = std::max(std::max(size_along_x, size_along_y), std::max(size_along_y, size_along_z));
    //float scaling_factor = scaling_fraction_numerator / scm::math::length(total_max_point - total_min_point);
    float scaling_factor = screen_width / (longest_axis*4.0);
    std::cout << scaling_factor << "SF\n";
    auto all_geometry_nodes = graph["/scene_root"]->get_children();
    for(auto& node : all_geometry_nodes){
      //auto tramformation_mat = node->get_world_transform();
      //auto local_transform = scm::math::make_translation(gua::math::get_translation(tramformation_mat));
      //auto local_transform = scm::math::make_translation(node->get_world_position());
      //auto local_transform_2 = node->get_geometry()->local_transform();
      //auto bbox_center_object_space = local_transform * gua::math::vec4(scene_bbox.center().x, scene_bbox.center().y, scene_bbox.center().z, 1.0);
      node->translate(-scene_bbox.center().x, -scene_bbox.center().y, -scene_bbox.center().z);
      //node->translate(0.0, 1.6, 0.0);
      node->scale(scaling_factor);
      node->set_draw_bounding_box(true);
    }
    
    
    //auto local_transform_2 = scm::math::make_translation(graph["/scene_root"]->get_world_position());
    //graph["/scene_root"]->update_cache();
    //graph["/scene_root"]->scale(scaling_factor);
    auto tramformation_mat = graph["/scene_root"]->get_world_transform();
    auto local_transform = scm::math::make_translation(gua::math::get_translation(tramformation_mat));
    auto bbox_center_object_space = local_transform * gua::math::vec4(scene_bbox.center().x, scene_bbox.center().y, scene_bbox.center().z, 1.0);
    //graph["/scene_root"]->translate(-scene_bbox.center().x, -scene_bbox.center().y, -scene_bbox.center().z);
   graph["/scene_root"]->set_draw_bounding_box(true);
    //std::cout <<  tramformation_mat  << " tramformation_mat \n"; 
    //std::cout <<  scaling_factor  << " scaling_factor \n";    
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
  // initialize guacamole
  gua::init(1, argv);

	//setup scene ////////////////////////////////////
	gua::SceneGraph graph("main_scenegraph");
	auto scene_transform = graph.add_node<gua::node::TransformNode>("/", "scene_root");
  auto initial_scene_translation_vec = scm::math::vec<double, 3u>(y_offset_scene_track_target, y_offset_scene_track_target, y_offset_scene_track_target);
  scene_transform->translate(initial_scene_translation_vec);
  //scene_transform->translate(initial_scene_translation_vec.x, initial_scene_translation_vec.y, initial_scene_translation_vec.z);
	auto light_pointer = graph.add_node<gua::node::TransformNode>("/", "light_pointer");
	auto light_source = graph.add_node<gua::node::LightNode>("/light_pointer", "light_source");
	light_source->data.set_type(gua::node::LightNode::Type::SUN);
	light_source->data.brightness = 3.0f;

  //TMP try loading sample models to see if window gets created
  //create simple untextured material shader
  auto lod_keep_input_desc = std::make_shared<gua::MaterialShaderDescription>("./data/materials/PLOD_use_input_color.gmd");
  auto lod_keep_color_shader(std::make_shared<gua::MaterialShader>("PLOD_pass_input_color", lod_keep_input_desc));
  gua::MaterialShaderDatabase::instance()->add(lod_keep_color_shader);
  auto lod_rough = lod_keep_color_shader->make_new_material();
  lod_rough->set_uniform("metalness", 0.5f);
  lod_rough->set_uniform("roughness", 1.0f);
  lod_rough->set_uniform("emissivity", 0.0f);
  gua::LodLoader lod_loader;
  lod_loader.set_out_of_core_budget_in_mb(8000);
  lod_loader.set_render_budget_in_mb(6000);
  lod_loader.set_upload_budget_in_mb(32);
  auto plod_head = lod_loader.load_lod_pointcloud("pointcloud_head",
                                                   "/mnt/pitoti/hallermann_scans/bruecke2/0_kopf_local.bvh",
                                                   lod_rough,
                                                   gua::LodLoader::NORMALIZE_POSITION | 
                                                   gua::LodLoader::NORMALIZE_SCALE);
  plod_head->scale(0.1);
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
  //auto screen_transform = graph.add_node<gua::node::TransformNode>("/", "screen_transform");
	auto screen = graph.add_node<gua::node::ScreenNode>("/", "screen");
  screen->data.set_size(gua::math::vec2(screen_width, screen_height));
  //////////////TMP
    
  #if USE_SIDE_BY_SIDE 
  screen->rotate(screen_rotation_x, 1, 0, 0);
  screen->rotate(screen_rotation_y, 0, 1, 0);
  //screen->rotate(screen_rotation_z, 0, 0, 1);
  screen->translate(screen_center_offset_x, screen_center_offset_y, screen_center_offset_z);
  #else
  screen->translate(0, 0, -0.6);
  #endif
  //////////////////////////////TMP

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
  camera->config.set_near_clip(0.01);
  camera->config.set_far_clip(3000.0);
  camera->config.set_eye_dist(eye_dist);

  //add pipeline with rendering passes
	auto pipe = std::make_shared<gua::PipelineDescription>();
	build_pipe(*pipe);
	camera->set_pipeline_description(pipe);

  //TMP
  //plod_head->set_draw_bounding_box(true);
  //graph.add_node(scene_transform, plod_head);

  //////////////////input handling addapted from Lamure /////////////
  namespace po = boost::program_options;
 
  std::string resource_file = "auto_generated.rsc";
  po::options_description desc("options: ");
  desc.add_options()
    ("help,h", "print help message")
    ("input,f", po::value<std::string>(), "specify input file with scene configuration")
    ("models,m", po::value <std::vector<std::string>>()->multitoken(), "list paths to desired input models")
  ;
  //po::positional_options_description p;
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

	  std::thread tracking_thread([&]() {
      scm::inp::tracker::target_container targets;
      targets.insert(scm::inp::tracker::target_container::value_type(5, scm::inp::target(5)));
      targets.insert(scm::inp::tracker::target_container::value_type(12, scm::inp::target(12)));

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

        if(!detach_scene_from_tracking_target) {
          auto scene_root_target = targets.find(12)->second.transform();
          scene_root_target[12] /= 1000.f; 
          scene_root_target[13] /= 1000.f; 
          scene_root_target[14] /= 1000.f;
          current_scene_tracking_matrix = scene_root_target;
        }
      }
    });
	#endif

  auto scene_transform_mat = gua::math::mat4::identity();
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
          if(!detach_scene_from_tracking_target){
            auto current_scene_translation_vec = gua::math::get_translation(current_scene_tracking_matrix);
            auto current_scene_rot_matrix = gua::math::get_rotation(current_scene_tracking_matrix);
            scene_transform_mat = scm::math::make_translation(current_scene_translation_vec)
                                  * current_scene_rot_matrix
                                  * scm::math::make_scale(zoom_factor, zoom_factor, zoom_factor);
            scene_transform->set_transform(scene_transform_mat);
          }          
        #endif
    		renderer.queue_draw({&graph});
    	}
	});

	loop.start();

	return 0;
}