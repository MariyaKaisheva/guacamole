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
#include <gua/renderer/NPREffectPass.hpp>
#include <gua/utils/Trackball.hpp>

#include <gua/renderer/TriMeshPass.hpp>
#include <gua/renderer/LightVisibilityPass.hpp>
#include <gua/renderer/BBoxPass.hpp>
#include <gua/renderer/TexturedQuadPass.hpp>
#include <gua/renderer/TexturedScreenSpaceQuadPass.hpp>
//#include <gua/renderer/SkeletalAnimationPass.hpp>


#define USE_ASUS_3D_WORKSTATION 1
#define USE_LOW_RES_WORKSTATION 0

#define USE_QUAD_BUFFERED 0
#define USE_ANAGLYPH 1 
#define USE_MONO 0


#define USE_TOON_RESOLVE_PASS 1

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

bool use_toon_resolve_pass = false; 
bool do_halftoning_effect = false; 
int thickness = 2;
int max_thickness = 5;
int min_thickness = 0;

//key-board interacions 
void key_press(gua::PipelineDescription& pipe, gua::SceneGraph& graph, int key, int scancode, int action, int mods)
{
  if (action == 0) return;
  switch(std::tolower(key)){
    
    //line thickness
    case 'w':      
     if(thickness < max_thickness)
      {++thickness;}
      pipe.get_npr_pass()->line_thickness(thickness);
    break;   

    case 's':   
      if(thickness > min_thickness)
       {--thickness;}
      pipe.get_npr_pass()->line_thickness(thickness);
    break; 

    //shading mode
    case 'g':

    //#if USE_TOON_RESOLVE_PASS
    if(!use_toon_resolve_pass){
      if(pipe.get_toon_resolve_pass()->enable_fog()) //temp substitute
        {pipe.get_toon_resolve_pass()->enable_fog(false);}
      else 
        {pipe.get_toon_resolve_pass()->enable_fog(true);}
    }
      
    //#endif

    break;

    //toogle resolve pass ??
    case 'r':
      use_toon_resolve_pass = !use_toon_resolve_pass; 
      pipe.clear();
      pipe.add_pass(std::make_shared<gua::TriMeshPassDescription>());
      //pipe->add_pass(std::make_shared<gua::SkeletalAnimationPassDescription>()); //ß
      pipe.add_pass(std::make_shared<gua::LightVisibilityPassDescription>());
      if(use_toon_resolve_pass){
        pipe.add_pass(std::make_shared<gua::ResolvePassDescription>());  
      }
      else{pipe.add_pass(std::make_shared<gua::ToonResolvePassDescription>());}
      pipe.add_pass(std::make_shared<gua::NPREffectPassDescription>());
      pipe.add_pass(std::make_shared<gua::DebugViewPassDescription>());      
    break;  
    
    //halftoning mode - screenspace pass
    case 'h':
      do_halftoning_effect = !do_halftoning_effect;
      pipe.get_npr_pass()->halftoning(do_halftoning_effect);      
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

 // gua::SkeletalAnimationLoader loader;//ß
  gua::TriMeshLoader loader;

  auto transform = graph.add_node<gua::node::TransformNode>("/", "transform");

  #if USE_TOON_RESOLVE_PASS
  auto snail_material(gua::MaterialShaderDatabase::instance()
                      ->lookup("gua_default_material")
                      ->make_new_material());

   snail_material->set_uniform("Roughness", 0.2f);
   snail_material->set_uniform("Metalness", 0.0f);

  std::string directory("data/textures/");
  snail_material->set_uniform("ColorMap", directory + "snail_color.png");
  #endif
  
/*pbrMat->set_uniform("MetalnessMap", directory + "Cerberus_M.tga");
  pbrMat->set_uniform("RoughnessMap", directory + "Cerberus_R.tga");
  pbrMat->set_uniform("NormalMap", directory + "Cerberus_N.negated_green.tga");*/

  #if USE_TOON_RESOLVE_PASS
  auto teapot(loader.create_geometry_from_file(
     // "teapot", "data/objects/teapot.obj",
     "teapot", "data/objects/ohluvka.obj",
      snail_material,
      gua::TriMeshLoader::NORMALIZE_POSITION |
          gua::TriMeshLoader::NORMALIZE_SCALE));
  #else
  auto teapot(loader.create_geometry_from_file(
      "teapot", "data/objects/teapot.obj",
      gua::TriMeshLoader::NORMALIZE_POSITION |
          gua::TriMeshLoader::NORMALIZE_SCALE));
  #endif
  graph.add_node("/transform", teapot);
  teapot->set_draw_bounding_box(true);


  auto light2 = graph.add_node<gua::node::LightNode>("/", "light2");
  light2->data.set_type(gua::node::LightNode::Type::POINT);
  light2->data.brightness = 150.0f;
  light2->scale(12.f);
  light2->translate(-3.f, 5.f, 5.f);

  auto screen = graph.add_node<gua::node::ScreenNode>("/", "screen");
  //physical size of output viewport

  #if USE_ASUS_3D_WORKSTATION
  screen->data.set_size(gua::math::vec2(0.598f, 0.336f));
  #else
  screen->data.set_size(gua::math::vec2(0.40f, 0.20f));
  #endif
  screen->translate(0, 0, 1.0);

  // add mouse interaction
  gua::utils::Trackball trackball(0.01, 0.002, 0.2);


  
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
  //npr_pass->line_thickness(thickness);

  


  auto camera = graph.add_node<gua::node::CameraNode>("/screen", "cam");
  camera->translate(0.0, 0, 2.0);
  camera->config.set_resolution(resolution);
  camera->config.set_screen_path("/screen");
  camera->config.set_scene_graph_name("main_scenegraph");
  camera->config.set_output_window_name("main_window");
  camera->config.set_near_clip(0.001);
  
  #if USE_MONO
  camera->config.set_enable_stereo(false);
  #else
  camera->config.set_enable_stereo(true);
  #endif

 // camera->get_pipeline_description()->get_resolve_pass()->tone_mapping_exposure(
  //  1.0f);


  auto pipe = std::make_shared<gua::PipelineDescription>();
  //auto pipe(std::make_shared<PipelineDescription>());

  pipe->add_pass(std::make_shared<gua::TriMeshPassDescription>());
  //camera->get_pipeline_description()->add_pass(std::make_shared<TexturedQuadPassDescription>());

  pipe->add_pass(std::make_shared<gua::LightVisibilityPassDescription>());

  pipe->add_pass(npr_resolve_pass);
  pipe->add_pass(npr_pass); //std::make_shared<gua::NPREffectPassDescription>());
  //camera->get_pipeline_description()->get_pass()->line_thickness(thickness); //set line thickness uniform?
  pipe->add_pass(
   std::make_shared<gua::DebugViewPassDescription>());

  camera->set_pipeline_description(pipe);

  #if USE_QUAD_BUFFERED
  auto window = std::make_shared<gua::Window>();
  #else
  auto window = std::make_shared<gua::GlfwWindow>();

  window->on_resize.connect([&](gua::math::vec2ui const& new_size) {
    window->config.set_resolution(new_size);
    camera->config.set_resolution(new_size);
    screen->data.set_size(
        gua::math::vec2(0.001 * new_size.x, 0.001 * new_size.y));
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


  unsigned passed_frames = 0;

  ticker.on_tick.connect([&]() {



    // apply trackball matrix to object

    #if USE_QUAD_BUFFERED
    gua::math::mat4 modelmatrix =  scm::math::make_rotation(++passed_frames/90.0, 0.0, 1.0, 0.0 );
    #else
    gua::math::mat4 modelmatrix =

        scm::math::make_translation(trackball.shiftx(), trackball.shifty(),
                                    trackball.distance()) *
        gua::math::mat4(trackball.rotation());
    #endif

        //scm::math::
    transform->set_transform( scm::math::make_translation(0.0, 0.0, 2.0) * modelmatrix *  scm::math::make_rotation(-90.0, 0.0, 1.0, 0.0) * scm::math::make_scale(0.4, 0.4, 0.4));

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
