/******************************************************************************
 * guacamole - delicious VR                                                   *
 *                                                                            *
 * Copyright: (c) 2011-2013 Bauhaus-Universität Weimar                        *
 * Contact:   felix.lauer@uni-weimar.de / simon.schneegans@uni-weimar.de      *
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

#include <functional>

#include <gua/guacamole.hpp>
#include <gua/renderer/TriMeshLoader.hpp>
#include <gua/renderer/ToneMappingPass.hpp>
#include <gua/renderer/DebugViewPass.hpp>
#include <gua/renderer/ToonResolvePass.hpp> 
#include <gua/renderer/NprOutlinePass.hpp>
#include <gua/renderer/NPREffectPass.hpp>
#include <gua/utils/Trackball.hpp>

#include <gua/renderer/TriMeshPass.hpp>
#include <gua/renderer/LightVisibilityPass.hpp>
#include <gua/renderer/BBoxPass.hpp>
#include <gua/renderer/TexturedQuadPass.hpp>
#include <gua/renderer/TexturedScreenSpaceQuadPass.hpp>
#include <gua/renderer/SkeletalAnimationPass.hpp>

#include <gua/renderer/LodLoader.hpp>
#include <gua/renderer/PLodPass.hpp>
#include <gua/node/PLodNode.hpp> 
#include <gua/node/TriMeshNode.hpp>

#include <gua/renderer/PBSMaterialFactory.hpp> 


#define USE_ASUS_3D_WORKSTATION 1
#define USE_LOW_RES_WORKSTATION 0

#define USE_QUAD_BUFFERED 0
#define USE_ANAGLYPH 1
#define USE_MONO 0 


#define USE_TOON_RESOLVE_PASS 0


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
////golbal variables
bool moves_positive_y = false;
bool moves_negative_y = false;
bool moves_positive_x = false;
bool moves_negative_x = false;

bool use_toon_resolve_pass = false; 
bool apply_bilateral_filter = false;
bool apply_halftoning_effect = false; 
bool create_screenspace_outlines = false; 
int thickness = 0;
int max_thickness = 15; //confusing name 
int min_thickness = 0; //confusing name 
float sigma_d = 0.1; //not used currently

int num_screenspace_passes = 1;

void rebuild_pipe(gua::PipelineDescription& pipe) {
  pipe.clear();
  pipe.add_pass(std::make_shared<gua::TriMeshPassDescription>());
  pipe.add_pass(std::make_shared<gua::PLodPassDescription>());
  pipe.add_pass(std::make_shared<gua::SkeletalAnimationPassDescription>()); 
  pipe.add_pass(std::make_shared<gua::LightVisibilityPassDescription>());



  if(!use_toon_resolve_pass){
    pipe.add_pass(std::make_shared<gua::ResolvePassDescription>());  
  }
  else{
    pipe.add_pass(std::make_shared<gua::ToonResolvePassDescription>());
  }
   
  /*if (apply_halftoning_effect){
    pipe.add_pass(std::make_shared<gua::NprOutlinePassDescription>());
    pipe.get_npr_outline_pass()->halftoning(apply_halftoning_effect);
  }*/

  if (apply_bilateral_filter){

    for(int i = 0; i < num_screenspace_passes; ++i){
      pipe.add_pass(std::make_shared<gua::NPREffectPassDescription>());

    }

  }
  
  if (create_screenspace_outlines || apply_halftoning_effect){
   pipe.add_pass(std::make_shared<gua::NprOutlinePassDescription>());
  pipe.get_npr_outline_pass()->halftoning(apply_halftoning_effect);
   pipe.get_npr_outline_pass()->apply_outline(create_screenspace_outlines);
  }

  //pipe.add_pass(std::make_shared<gua::NprOutlinePassDescription>());
  //pipe.add_pass(std::make_shared<gua::DebugViewPassDescription>());

}

//key-board interacions 
void key_press(gua::PipelineDescription& pipe, gua::SceneGraph& graph, int key, int scancode, int action, int mods)
{

  //std::cout << "scancode: " << scancode << " with action " << action <<":\n";
  
  bool movement_predicate = false;

  if( action != 0 ) {
    movement_predicate = true;
  } 

  if(80 == scancode) {
    moves_positive_y = movement_predicate;
  }

  if(88 == scancode ) {
    moves_negative_y = movement_predicate;
  } 

  if(83 == scancode) {
    moves_negative_x = movement_predicate;
  }

  if(85 == scancode) {
    moves_positive_x = movement_predicate;
  }
 
  if (action == 0) return;
  switch(std::tolower(key)){
    
    //line thickness
    case 'w':      
      if(thickness < max_thickness && apply_bilateral_filter) {
        ++thickness;
        pipe.get_npr_pass()->line_thickness(thickness);
      }
    break;   

    case 's':   
      if(thickness > min_thickness && apply_bilateral_filter) {
        --thickness;
        pipe.get_npr_pass()->line_thickness(thickness);
      }
    break; 


    case 'd':
       num_screenspace_passes +=1; 
        rebuild_pipe(pipe);   
                      /*if(sigma_d < 10.0){
                        sigma_d += 1;
                      }
                      else{
                        sigma_d = 0.1;
                      }
                       pipe.get_npr_pass()->sigma_d(sigma_d);*/
    break;

    case 'c':
      if(num_screenspace_passes > 1){
        num_screenspace_passes -=1; 
          rebuild_pipe(pipe);   
      }
    break;

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
  
   default:
    break;   
  }
}

int main(int argc, char** argv) {
  // initialize guacamole
  gua::init(argc, argv);

  // setup scene
  gua::SceneGraph graph("main_scenegraph");

  gua::SkeletalAnimationLoader loader; //change name later
  gua::TriMeshLoader   trimesh_loader;
  gua::LodLoader lod_loader;
  lod_loader.set_out_of_core_budget_in_mb(512);
  lod_loader.set_render_budget_in_mb(512);
  lod_loader.set_upload_budget_in_mb(20);

  auto transform = graph.add_node<gua::node::TransformNode>("/", "transform");


  //materials///////////////
  
  auto snail_material(gua::MaterialShaderDatabase::instance()
                      ->lookup("gua_default_material")
                      ->make_new_material());

   snail_material->set_uniform("Roughness", 0.2f);
   snail_material->set_uniform("Metalness", 0.0f);

  std::string directory("data/textures/");
  snail_material->set_uniform("ColorMap", directory + "snail_color.png");
  snail_material->set_uniform("Emissivity", 0.0f);
 
 //material for animated character
  auto mat1(gua::PBSMaterialFactory::create_material(static_cast<gua::PBSMaterialFactory::Capabilities>(gua::PBSMaterialFactory::ALL)));
  mat1->set_uniform("Metalness", 0.0f);
  mat1->set_uniform("Roughness", 0.7f);
  mat1->set_uniform("Emissivity", 0.0f);  

  
   //create simple untextured material shader
  auto lod_keep_input_desc = std::make_shared<gua::MaterialShaderDescription>("./data/materials/PLOD_use_input_color.gmd");
  auto lod_keep_color_shader(std::make_shared<gua::MaterialShader>("PLOD_pass_input_color", lod_keep_input_desc));
  gua::MaterialShaderDatabase::instance()->add(lod_keep_color_shader);
  //material for pointcloud


  //add geometry/////////////

  //---snail----
  auto snail_node(trimesh_loader.create_geometry_from_file(
     "teapot", "data/objects/ohluvka.obj",
      snail_material,
      gua::TriMeshLoader::NORMALIZE_POSITION |
          gua::TriMeshLoader::NORMALIZE_SCALE));

  //---Sponsa---
  auto scene_node(trimesh_loader.create_geometry_from_file("sponsa", "/home/vajo3185/Modified_Sponza/sponza.obj", gua::TriMeshLoader::NORMALIZE_POSITION /*|  gua::TriMeshLoader::NORMALIZE_SCALE*/ | gua::TriMeshLoader::LOAD_MATERIALS ));


  //---teapod----
  /*auto teapot(trimesh_loader.create_geometry_from_file(
      "teapot", "data/objects/teapot.obj",
      gua::TriMeshLoader::NORMALIZE_POSITION |
          gua::TriMeshLoader::NORMALIZE_SCALE));*/

   auto sphere(trimesh_loader.create_geometry_from_file(
                "icosphere", "data/objects/icosphere.obj", 
                gua::TriMeshLoader::NORMALIZE_POSITION |
                gua::TriMeshLoader::NORMALIZE_SCALE));   

  //---character---    
  auto teapot(loader.create_geometry_from_file("teapot", "/opt/project_animation/Assets/HeroTPP.FBX",
                       mat1, 
                       gua::SkeletalAnimationLoader::NORMALIZE_POSITION | 
                       gua::SkeletalAnimationLoader::NORMALIZE_SCALE)); 


  teapot->set_transform(scm::math::make_translation(1.0, 0.0, 0.0)*scm::math::make_rotation(-90.0, 1.0, 0.0, 0.0) * teapot->get_transform());
  teapot->add_animations("/opt/project_animation/Assets/Walk.FBX", "walk");
  teapot->set_animation_1("walk");
  // play only anim nr. 1
  teapot->set_blend_factor(0);


  auto lod_rough = lod_keep_color_shader->make_new_material();
  lod_rough->set_uniform("metalness", 0.0f);
  lod_rough->set_uniform("roughness", 1.0f);
  lod_rough->set_uniform("emissivity", 1.0f);


   //---point cloud---   
   auto plod_node = lod_loader.load_lod_pointcloud("pointcloud",
                                                   "/mnt/pitoti/hallermann_scans/bruecke2/kopf_local.bvh",
                                                  //"/mnt/pitoti/hallermann_scans/Schiefer_Turm_part_200M_00001_knobi.bvh",
                                                   lod_rough,
                                                   gua::LodLoader::NORMALIZE_POSITION);

   snail_node->set_transform(scm::math::make_scale(5.0, 5.0,5.0));
   plod_node->set_radius_scale(1.3f);


  graph.add_node("/transform", scene_node);  
  graph.add_node("/transform", plod_node);
  teapot->set_draw_bounding_box(true);


  auto light2 = graph.add_node<gua::node::LightNode>("/", "light2");
  light2->data.set_type(gua::node::LightNode::Type::POINT);
  light2->data.brightness = 1500000.0f;
  light2->scale(800.f);
  light2->translate(-3.f, 5.f, 5.f);
  graph.add_node("/light2", sphere);




  
  // setup rendering pipeline and window

  gua::math::vec2ui resolution;

  #if USE_ASUS_3D_WORKSTATION
  resolution = gua::math::vec2ui(2560, 1440);
  #else
  resolution = gua::math::vec2ui(1920, 1080);
  #endif



  #if USE_TOON_RESOLVE_PASS
    auto npr_resolve_pass = std::make_shared<gua::ToonResolvePassDescription>();
  #else
    auto npr_resolve_pass = std::make_shared<gua::ResolvePassDescription>();
  npr_resolve_pass->background_mode(
      gua::ResolvePassDescription::BackgroundMode::QUAD_TEXTURE);
  #endif
  npr_resolve_pass->tone_mapping_exposure(1.0f);
  auto npr_pass = std::make_shared<gua::NPREffectPassDescription>();


  auto navigation = graph.add_node<gua::node::TransformNode>("/", "navigation");

  auto screen = graph.add_node<gua::node::ScreenNode>("/navigation", "screen");
  //physical size of output viewport

  #if USE_ASUS_3D_WORKSTATION
  screen->data.set_size(gua::math::vec2(0.598f, 0.336f));
  #else
  screen->data.set_size(gua::math::vec2(0.40f, 0.20f));
  #endif
  screen->translate(0, 0, 1.0);

  auto camera = graph.add_node<gua::node::CameraNode>("/navigation/screen", "cam");
  camera->translate(0.0, 0, 2.0);
  camera->config.set_resolution(resolution);
  camera->config.set_screen_path("/navigation/screen");
  camera->config.set_scene_graph_name("main_scenegraph");
  camera->config.set_output_window_name("main_window");
  camera->config.set_near_clip(0.1);
  camera->config.set_far_clip(3000.0);
  
  #if USE_MONO
  camera->config.set_enable_stereo(false);
  #else
  camera->config.set_enable_stereo(true);
  #endif

  // add mouse interaction
  gua::utils::Trackball trackball(0.1, 0.02, 0.02);

 // camera->get_pipeline_description()->get_resolve_pass()->tone_mapping_exposure(
  //  1.0f);


  auto pipe = std::make_shared<gua::PipelineDescription>();
  rebuild_pipe(*pipe);

  camera->set_pipeline_description(pipe);

  #if USE_QUAD_BUFFERED
  auto window = std::make_shared<gua::Window>();
  #else
  auto window = std::make_shared<gua::GlfwWindow>();

  window->on_resize.connect([&](gua::math::vec2ui const& new_size) {
    window->config.set_resolution(new_size);
    camera->config.set_resolution(new_size);
    screen->data.set_size(gua::math::vec2(0.001 * new_size.x, 0.001 * new_size.y));
  });

  window->on_move_cursor.connect(
      [&](gua::math::vec2 const& pos) { trackball.motion(pos.x, pos.y); });
  window->on_button_press.connect(
      std::bind(mouse_button, std::ref(trackball), std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3));

  window->on_key_press.connect(std::bind(key_press,
    std::ref(*(camera->get_pipeline_description())),
    std::ref(graph),
    std::placeholders::_1,
    std::placeholders::_2,
    std::placeholders::_3,
    std::placeholders::_4));

  #endif


  gua::WindowDatabase::instance()->add("main_window", window);

  window->config.set_enable_vsync(false);
  window->config.set_size(resolution);

  window->config.set_resolution(resolution);

  #if USE_QUAD_BUFFERED
  window->config.set_stereo_mode(gua::StereoMode::QUAD_BUFFERED);
  #endif

  #if USE_ANAGLYPH
  window->config.set_stereo_mode(gua::StereoMode::ANAGLYPH_RED_CYAN);
  #endif

  #if USE_MONO
  window->config.set_stereo_mode(gua::StereoMode::MONO);
  #endif


  gua::Renderer renderer;

  // application loop
  gua::events::MainLoop loop;
  gua::events::Ticker ticker(loop, 1.0 / 500.0);


  auto default_node_transform = plod_node->get_transform();

  unsigned passed_frames = 0;
  float i = 0; 
  double current_scaling = 1.0;
  ticker.on_tick.connect([&]() {


    double amount = 1.0 / 5.0;

    if( moves_negative_y ) 
      navigation->translate(0.0, amount, 0.0);
    if( moves_positive_y ) 
      navigation->translate(0.0, -amount, 0.0);

  
    if( moves_positive_x ) 
      navigation->translate(0.0, 0.0, -amount);
    if( moves_negative_x ) 
      navigation->translate(0.0, 0.0,  amount);
    
    // set time variable for animation
    i += 1.0 / 600.0;
    if (i > 1) i = 0;
    teapot->set_time_1(i);

    // apply trackball matrix to object

    #if USE_QUAD_BUFFERED
    gua::math::mat4 modelmatrix =  scm::math::make_rotation(++passed_frames/90.0, 0.0, 1.0, 0.0 );
    #else
    gua::math::mat4 modelmatrix = scm::math::make_translation(trackball.shiftx(), trackball.shifty(),trackball.distance()) *
                                  gua::math::mat4(trackball.rotation());
    #endif


     gua::math::mat4 scale_mat = scm::math::make_scale(200.0, 200.0, 200.0);
     gua::math::mat4 rot_mat_x = scm::math::make_rotation(-90.0, 1.0, 0.0, 0.0);
     gua::math::mat4 rot_mat_y = scm::math::make_rotation(106.8, 0.0, 1.0, 0.0);
     gua::math::mat4 trans_mat = scm::math::make_translation(-1330.0, -15.0, -10.0);
     plod_node->set_transform( trans_mat*rot_mat_y*rot_mat_x* scale_mat *  default_node_transform);



    transform->set_transform(scm::math::make_translation(0.0, 0.0, 2.0) * modelmatrix *  scm::math::make_rotation(-90.0, 0.0, 1.0, 0.0)* scm::math::make_scale(0.4, 0.4, 0.4));

    //gua::math::mat4 plod_node_trasnformations = plod_node->get_trasform();
    
    std::cout << "\n\n\nNew frame: " << "\n";

    if (window->should_close()) {
      renderer.stop();
      window->close();
      loop.stop();
    } else {
      renderer.queue_draw({&graph});
    }



  });

  loop.start();

  return 0;
}
