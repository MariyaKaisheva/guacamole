
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


#include <scm/input/tracking/art_dtrack.h>
#include <scm/input/tracking/target.h>

#include <gua/guacamole.hpp>
#include <gua/renderer/TriMeshLoader.hpp>
#include <gua/renderer/ToneMappingPass.hpp>
#include <gua/renderer/DebugViewPass.hpp>
#include <gua/renderer/SSAAPass.hpp>
#include <gua/renderer/ToonResolvePass.hpp> 
#include <gua/renderer/NprOutlinePass.hpp>
#include <gua/renderer/NPREffectPass.hpp>
#include <gua/utils/Trackball.hpp>
//#include <gua/gui.hpp> 

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

//#define USE_ASUS_3D_WORKSTATION 1
#define USE_LOW_RES_WORKSTATION 0

//#define USE_QUAD_BUFFERED 0//seems to not work anymore
#define USE_SIDE_BY_SIDE 1
#define USE_ANAGLYPH 0
#define USE_MONO 0 

#define TRACKING_ENABLED 1

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
bool close_window = false;
bool show_scene_1 = true;
bool show_scene_2 = true;
bool moves_positive_z = false;
bool moves_negative_z = false;
bool moves_positive_y = false;
bool moves_negative_y = false;
bool moves_positive_x = false;
bool moves_negative_x = false;
bool reset_position = false;

bool use_toon_resolve_pass = false; 
bool apply_bilateral_filter = false;
bool apply_halftoning_effect = false; 
bool no_color = false;
bool create_screenspace_outlines = false; 
int thickness = 0;
//int brightness_factor = 1;
int max = 15; //max num applications of Bilateral filter
int min = 0; //min num applications of Bilateral filter
//float sigma_d = 0.1; //not used currently

int num_screenspace_passes = 1;


// physical set-up for SBS stereo workstation 
float screen_width = 0.595f;
float screen_height = 0.3346f;
//float screen_distance = 0.6f;

float screen_offset_x = 3.192f;
float screen_offset_y = 1.2125f;
float screen_offset_z = 1.148f;

float screen_rotation_x = -12.46f;
float screen_rotation_y = -90.0f;
float screen_rotation_z = 0.0f;

float eye_dist = 0.06;
float glass_eye_offset = 0.03f;

int   window_width = 2560;
int   window_height = 1440;



void rebuild_pipe(gua::PipelineDescription& pipe) {
  pipe.clear();
  pipe.add_pass(std::make_shared<gua::TriMeshPassDescription>());
  pipe.add_pass(std::make_shared<gua::PLodPassDescription>());
 //pipe.add_pass(std::make_shared<gua::SkeletalAnimationPassDescription>()); 
  pipe.add_pass(std::make_shared<gua::LightVisibilityPassDescription>());



  if(!use_toon_resolve_pass){
    pipe.add_pass(std::make_shared<gua::ResolvePassDescription>());  
  }
  else{
    pipe.add_pass(std::make_shared<gua::ToonResolvePassDescription>());
  }
   
  if (apply_bilateral_filter){

    for(int i = 0; i < num_screenspace_passes; ++i){
      pipe.add_pass(std::make_shared<gua::NPREffectPassDescription>());

    }

  }
  
  if (create_screenspace_outlines || apply_halftoning_effect){
   pipe.add_pass(std::make_shared<gua::NprOutlinePassDescription>());
   pipe.get_npr_outline_pass()->halftoning(apply_halftoning_effect);
   pipe.get_npr_outline_pass()->apply_outline(create_screenspace_outlines);
   pipe.get_npr_outline_pass()->no_color(no_color);
  }

  //pipe.add_pass(std::make_shared<gua::NprOutlinePassDescription>());
  //pipe.add_pass(std::make_shared<gua::DebugViewPassDescription>());
  pipe.add_pass(std::make_shared<gua::SSAAPassDescription>());

}
////////////////////////////////////////////////////////////////////////////////////////////////////////////

//key-board interacions 
void key_press(gua::PipelineDescription& pipe, gua::SceneGraph& graph, int key, int scancode, int action, int mods)
{

  
  
  bool movement_predicate = false;
  std::cout << "scancode: " << scancode << " with action " << action <<"\n";

  if( action != 0 ) {
    movement_predicate = true;
  } 

  if(25 == scancode){ // 'w' (111 - up arrow)
    moves_positive_z = movement_predicate;
  }

  if(39 == scancode){//'s' (116 - down arrow)
    moves_negative_z = movement_predicate;
  }

  if(38 == scancode){ //'a' (113 left arrow) 
    moves_positive_x = movement_predicate;
  }

  if(40 == scancode){//'d' (114 right arrow)
    moves_negative_x = movement_predicate;
  }

  if(90 == scancode){ //Numpad 0 pressed
    reset_position = true;
  }

  if(9 == scancode){ //'Esc'
      close_window = true;
  }

  //scene switch
  if(10 == scancode && action == 1){ //'1' hide sponza scene                 
    if(!graph["/transform/sponza_scene_transform"]->get_tags().has_tag("invisible")){
      graph["/transform/sponza_scene_transform"]->get_tags().add_tag("invisible");
    }
    else{
      graph["/transform/sponza_scene_transform"]->get_tags().remove_tag("invisible");
    }
  }
  if(11 == scancode && action == 1){ //'2' hide  pointcloud scene
    if(!graph["/transform/plod_scene_transform"]->get_tags().has_tag("invisible")){
      graph["/transform/plod_scene_transform"]->get_tags().add_tag("invisible");
    }
    else{
      graph["/transform/plod_scene_transform"]->get_tags().remove_tag("invisible");
    }
  }

  if(12 == scancode && action == 1){ //'3' hide  simple geometry 
    if(!graph["/transform/simple_geom_scene_transform"]->get_tags().has_tag("invisible")){
      graph["/transform/simple_geom_scene_transform"]->get_tags().add_tag("invisible");
    }
    else{
      graph["/transform/simple_geom_scene_transform"]->get_tags().remove_tag("invisible");
    }
  }

  if(13 == scancode){ //'4' combined scene
    graph["/transform/sponza_scene_transform"]->get_tags().remove_tag("invisible");
    graph["/transform/plod_scene_transform"]->get_tags().remove_tag("invisible");
    graph["/transform/simple_geom_scene_transform"]->get_tags().remove_tag("invisible");
  }
 
  if (action == 0) return;
  switch(std::tolower(key)){

    case 'o':
      use_toon_resolve_pass = false;
      create_screenspace_outlines = false; 
      apply_bilateral_filter = false; 
      apply_halftoning_effect = false;
      rebuild_pipe(pipe);
    break;  
    
    //line thickness
    case 'c':
      if(thickness < max && apply_bilateral_filter) {
        ++thickness;
        /*++brightness_factor;
        pipe.get_toon_resolve_pass()->set_brightness_fact(brightness_factor); */
        pipe.get_npr_pass()->line_thickness(thickness);
      }
    break;   

    case 'v':   
      if(thickness > min && apply_bilateral_filter) {
        --thickness;
        pipe.get_npr_pass()->line_thickness(thickness);
      }
    break; 


    case 'n':
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

    case 'm':
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

    case 'k':
      no_color = !no_color;
      rebuild_pipe(pipe);
    break;
  
   default:
    break;
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
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

  //sponza scene
  auto sponza_scene_transform = graph.add_node<gua::node::TransformNode>("/transform", "sponza_scene_transform");

  //pointcloud scene
  auto plod_scene_transform = graph.add_node<gua::node::TransformNode>("/transform", "plod_scene_transform");
  auto plod_transform = graph.add_node<gua::node::TransformNode>("/transform/plod_scene_transform", "plod_transform");

  //simple geometry
  auto simple_geom_scene_transform = graph.add_node<gua::node::TransformNode>("/transform", "simple_geom_scene_transform");


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
  auto snail(trimesh_loader.create_geometry_from_file(
     "snail", "data/objects/ohluvka.obj",
      snail_material,
      gua::TriMeshLoader::NORMALIZE_POSITION |
          gua::TriMeshLoader::NORMALIZE_SCALE));

  //---Sponsa---
  auto sponza(trimesh_loader.create_geometry_from_file("sponza", "/home/vajo3185/More_Modified_Sponza_/sponza.obj", gua::TriMeshLoader::NORMALIZE_POSITION |  gua::TriMeshLoader::NORMALIZE_SCALE | 
                  gua::TriMeshLoader::LOAD_MATERIALS ));
  auto test_cube_with_colors(trimesh_loader.create_geometry_from_file("cube", "./data/colored_cube/colored_cube.obj", gua::TriMeshLoader::NORMALIZE_POSITION | gua::TriMeshLoader::LOAD_MATERIALS));
  auto test_cube_plain(trimesh_loader.create_geometry_from_file("cube", "./data/colored_cube/colored_cube.obj", gua::TriMeshLoader::NORMALIZE_POSITION ));
  auto test_cube_with_trancsparency(trimesh_loader.create_geometry_from_file("cube", "./data/colored_cube/colored_cube.obj", snail_material, gua::TriMeshLoader::NORMALIZE_POSITION ));
  
  //---teapot----
  auto teapot(trimesh_loader.create_geometry_from_file(
      "teapot", "data/objects/teapot.obj",
      gua::TriMeshLoader::NORMALIZE_POSITION |
          gua::TriMeshLoader::NORMALIZE_SCALE));

  //---plane----
  auto plane(trimesh_loader.create_geometry_from_file("plane", "data/objects/plane_4b4.obj", gua::TriMeshLoader::NORMALIZE_POSITION | gua::TriMeshLoader::LOAD_MATERIALS));

  //---cat----
   auto cat(trimesh_loader.create_geometry_from_file(
                    "cat", "/home/vajo3185/cat/cat.obj",
                    gua::TriMeshLoader::NORMALIZE_POSITION |
                    gua::TriMeshLoader::NORMALIZE_SCALE | 
                    gua::TriMeshLoader::LOAD_MATERIALS)); 

   //---light ball----
   auto sphere(trimesh_loader.create_geometry_from_file(
                "icosphere", "data/objects/icosphere.obj", 
                gua::TriMeshLoader::NORMALIZE_POSITION |
                gua::TriMeshLoader::NORMALIZE_SCALE |
                gua::TriMeshLoader::LOAD_MATERIALS));   

  //---character---    
  auto character(loader.create_geometry_from_file("character", "/opt/project_animation/Assets/HeroTPP.FBX",
                       mat1, 
                       gua::SkeletalAnimationLoader::NORMALIZE_POSITION | 
                       gua::SkeletalAnimationLoader::NORMALIZE_SCALE)); 


  character->set_transform(scm::math::make_translation(1.0, 0.0, 0.0)*scm::math::make_rotation(-90.0, 1.0, 0.0, 0.0) * character->get_transform());
  character->add_animations("/opt/project_animation/Assets/Walk.FBX", "walk");
  character->set_animation_1("walk");
  // play only anim nr. 1
  character->set_blend_factor(0);


  auto lod_rough = lod_keep_color_shader->make_new_material();
  lod_rough->set_uniform("metalness", 0.0f);
  lod_rough->set_uniform("roughness", 1.0f);
  lod_rough->set_uniform("emissivity", 1.0f);


   //---point cloud---   
   auto plod_head = lod_loader.load_lod_pointcloud("pointcloud_head",
                                                   "/mnt/pitoti/hallermann_scans/bruecke2/kopf_local.bvh",
                                                  //"/mnt/pitoti/hallermann_scans/Schiefer_Turm_part_200M_00001_knobi.bvh",
                                                   lod_rough,
                                                   gua::LodLoader::NORMALIZE_POSITION);

    auto plod_tower = lod_loader.load_lod_pointcloud("pointcloud_tower",
                                                   // "/mnt/pitoti/hallermann_scans/Schiefer_Turm_part_200M_00001_knobi.bvh",
                                                     "/mnt/pitoti/hallermann_scans/Modell_Ruine/Pointcloud_Ruine_xyz_parts_00001.bvh",
                                                       lod_rough);
    auto plod_tower_2 = lod_loader.load_lod_pointcloud("pointcloud_tower",
                                                      //"/mnt/pitoti/hallermann_scans/Schiefer_Turm_part_200M_00002_knobi.bvh",
                                                        "/mnt/pitoti/hallermann_scans/Modell_Ruine/Pointcloud_Ruine_xyz_parts_00002.bvh",
                                                       lod_rough);
    auto plod_tower_3 = lod_loader.load_lod_pointcloud("pointcloud_tower",
                                                      // "/mnt/pitoti/hallermann_scans/Schiefer_Turm_part_200M_00003_knobi.bvh",
                                                        "/mnt/pitoti/hallermann_scans/Modell_Ruine/Pointcloud_Ruine_xyz_parts_00003.bvh",
                                                       lod_rough);
    auto plod_tower_4 = lod_loader.load_lod_pointcloud("pointcloud_tower",
                                                      //"/mnt/pitoti/hallermann_scans/Schiefer_Turm_part_200M_00004_knobi.bvh",
                                                        "/mnt/pitoti/hallermann_scans/Modell_Ruine/Pointcloud_Ruine_xyz_parts_00004.bvh",
                                                       lod_rough);
    auto plod_tower_5 = lod_loader.load_lod_pointcloud("pointcloud_tower",
                                                     // "/mnt/pitoti/hallermann_scans/Schiefer_Turm_part_200M_00005_knobi.bvh",
                                                        "/mnt/pitoti/hallermann_scans/Modell_Ruine/Pointcloud_Ruine_xyz_parts_00005.bvh",
                                                       lod_rough);
    auto plod_tower_6 = lod_loader.load_lod_pointcloud("pointcloud_tower",
                                                      //"/mnt/pitoti/hallermann_scans/Schiefer_Turm_part_200M_00006_knobi.bvh",
                                                        "/mnt/pitoti/hallermann_scans/Modell_Ruine/Pointcloud_Ruine_xyz_parts_00006.bvh",
                                                       lod_rough);
    auto plod_tower_7 = lod_loader.load_lod_pointcloud("pointcloud_tower",
                                                      //"/mnt/pitoti/hallermann_scans/Schiefer_Turm_part_200M_00007_knobi.bvh",
                                                        "/mnt/pitoti/hallermann_scans/Modell_Ruine/Pointcloud_Ruine_xyz_parts_00007.bvh",
                                                       lod_rough);                                                                                              
    auto plod_tower_8 = lod_loader.load_lod_pointcloud("pointcloud_tower",
                                                      //"/mnt/pitoti/hallermann_scans/Schiefer_Turm_part_200M_00008_knobi.bvh",
                                                        "/mnt/pitoti/hallermann_scans/Modell_Ruine/Pointcloud_Ruine_xyz_parts_00008.bvh",
                                                       lod_rough);


   snail->set_transform(scm::math::make_scale(5.0, 5.0,5.0));
   //cat->set_transform(scm::math::make_translation(-80.0, -660.0, -70.0)*scm::math::make_rotation(65.0, 0.0, 1.0, 0.0)*scm::math::make_scale(150.0, 150.0, 150.0));
   cat->set_transform(scm::math::make_translation(1.5, -0.2, 0.0));
   plod_transform->rotate(-180, 1.0, 0.0, 0.0);
   plod_transform->rotate(-120, 0.0, 1.0, 0.0);
   plod_transform->translate(0.0, 14.0, 0.0);
   plod_transform->translate(50.0, 0.0, 15.0);
   plod_transform->scale(0.09);
   float scale_value = 0.7f;
   plod_head->set_radius_scale(scale_value);
   plod_tower->set_radius_scale(scale_value);
   plod_tower_2->set_radius_scale(scale_value);
   plod_tower_3->set_radius_scale(scale_value);
   plod_tower_4->set_radius_scale(scale_value);
   plod_tower_5->set_radius_scale(scale_value);
   plod_tower_6->set_radius_scale(scale_value);
   plod_tower_7->set_radius_scale(scale_value);
   plod_tower_8->set_radius_scale(scale_value);

   simple_geom_scene_transform->translate(0.20, 0.0, 0.0);
   //simple_geom_scene_transform->scale(2.5);
   sphere->translate(3.0, -0.5, 0.8);
   test_cube_plain->scale(0.05f);
   test_cube_plain->translate(0.3, 0.05, 0.0);
   test_cube_with_colors->translate(0.0, 0.10, 1.8);
   test_cube_with_colors->scale(0.05f);
   test_cube_with_trancsparency->translate(0.2, -0.05, -0.5);
   test_cube_with_trancsparency->scale(0.08f);

   

   #if USE_SIDE_BY_SIDE
    //transform->translate(screen_offset_x, screen_offset_y, screen_offset_z);
    sponza->scale(8.0f);
    sponza->rotate(180, 0, 1, 0);
    sponza->translate(screen_offset_x, screen_offset_y, screen_offset_z);
    sphere->scale(0.05);
   #endif

  simple_geom_scene_transform->translate(screen_offset_x, screen_offset_y, screen_offset_z);
  //teapot->translate(screen_offset_x, screen_offset_y, screen_offset_z);
  //teapot->translate(2.90, 0.0, 0.0);
  graph.add_node(sponza_scene_transform, sponza);
  graph.add_node(simple_geom_scene_transform, sphere);
  graph.add_node(simple_geom_scene_transform, test_cube_with_trancsparency);
  graph.add_node(simple_geom_scene_transform, test_cube_plain);
  graph.add_node(simple_geom_scene_transform, test_cube_with_colors);
  //graph.add_node(transform, plod_head);

  graph.add_node(plod_transform, plod_tower);
  graph.add_node(plod_transform, plod_tower_2);
  graph.add_node(plod_transform, plod_tower_3);
  graph.add_node(plod_transform, plod_tower_4);
  graph.add_node(plod_transform, plod_tower_5);
  graph.add_node(plod_transform, plod_tower_6);
  graph.add_node(plod_transform, plod_tower_7);
  graph.add_node(plod_transform, plod_tower_8);
  graph.add_node(transform, cat);
  //graph.add_node(plod_transform, plane);
  
  /*if(!show_scene_2){
    graph["/transform/plod_transform"]->get_tags().add_tag("invisible");
  }*/

 /* auto light2 = graph.add_node<gua::node::LightNode>("/", "light2");
  light2->data.set_type(gua::node::LightNode::Type::POINT);
  light2->data.brightness = 1500.0f;
  light2->data.color = gua::utils::Color3f(0.0, 1.0, 0.5);
  //light2->scale(800.f);
  light2->scale(12.f); //tmp
  light2->translate(-2.f, 5.f, 4.f);
  graph.add_node("/light2", sphere);*/

  
  // setup rendering pipeline and window

  gua::math::vec2ui resolution;

  #if !USE_LOW_RES_WORKSTATION
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
  //npr_resolve_pass->tone_mapping_exposure(1.0f);
  auto npr_pass = std::make_shared<gua::NPREffectPassDescription>();

  //////////////////////////////////
  auto navigation = graph.add_node<gua::node::TransformNode>("/", "navigation");
  auto light_pointer = graph.add_node<gua::node::TransformNode>("/navigation", "light_pointer");
  auto point_light = graph.add_node<gua::node::LightNode>("/navigation/light_pointer", "point_light");
  point_light->data.set_type(gua::node::LightNode::Type::SUN);
  point_light->data.brightness = 3.0f;
  //point_light->data.falloff = 1.5f;
  //point_light->data.brightness = 15.0f;
  point_light->data.color = gua::utils::Color3f(0.2, 1.0, 0.5);
  //graph.add_node(point_light, sphere);



  auto screen = graph.add_node<gua::node::ScreenNode>("/navigation", "screen");
   
  
  //physical size of output viewport

  /*#if USE_ASUS_3D_WORKSTATION
  screen->data.set_size(gua::math::vec2(0.598f, 0.336f));
  screen->translate(0, 0, -0.6);
  #else
  //screen->data.set_size(gua::math::vec2(0.40f, 0.20f));
  screen->translate(0, 0, -0.6);
  //screen->translate(0, 0, 1.0);
  #endif*/


  screen->data.set_size(gua::math::vec2(screen_width, screen_height)); //TODO check dim for other screens
  screen->translate(0, 0, -0.6);
  #if USE_SIDE_BY_SIDE  
  screen->rotate(screen_rotation_x, 1, 0, 0);
  screen->rotate(screen_rotation_y, 0, 1, 0);
  screen->translate(screen_offset_x, screen_offset_y, screen_offset_z);
  auto navigation_eye_offset = graph.add_node<gua::node::TransformNode>("/navigation", "eye_offset");
  auto camera = graph.add_node<gua::node::CameraNode>("/navigation/eye_offset", "cam");
  navigation_eye_offset->translate(0, 0, glass_eye_offset);
  camera->translate(screen_offset_x, screen_offset_y, screen_offset_z);
  #else 
  auto camera = graph.add_node<gua::node::CameraNode>("/navigation/screen", "cam");
  camera->translate(0.0, 0.0, 2.6);
  #endif
  camera->config.set_resolution(resolution);
  camera->config.set_screen_path("/navigation/screen");
  camera->config.set_scene_graph_name("main_scenegraph");
  camera->config.set_output_window_name("main_window");
  camera->config.set_near_clip(0.2);
  camera->config.set_far_clip(3000.0);
  camera->config.set_eye_dist(eye_dist);
  camera->config.mask().blacklist.add_tag("invisible");

  
  #if USE_MONO 
  camera->config.set_enable_stereo(false);
  #else
  camera->config.set_enable_stereo(true);
  #endif

  // add mouse interaction
  gua::utils::Trackball trackball(0.1, 0.02, 0.02); 

  auto pipe = std::make_shared<gua::PipelineDescription>();
  rebuild_pipe(*pipe);
  camera->set_pipeline_description(pipe);

  /*#if USE_QUAD_BUFFERED
  auto window = std::make_shared<gua::Window>();
  #else*/
  auto window = std::make_shared<gua::GlfwWindow>();

  #if !USE_SIDE_BY_SIDE
  window->on_resize.connect([&](gua::math::vec2ui const& new_size) {
    window->config.set_resolution(new_size);
    camera->config.set_resolution(new_size);
    screen->data.set_size(gua::math::vec2(0.001 * new_size.x, 0.001 * new_size.y));
  });
  #endif

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
  //#endif

  gua::WindowDatabase::instance()->add("main_window", window);
  window->config.set_enable_vsync(false);
  window->config.set_size(resolution);
  window->config.set_resolution(resolution);

  #if USE_QUAD_BUFFERED
  window->config.set_stereo_mode(gua::StereoMode::QUAD_BUFFERED);
  #endif

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


  //------------------tracking--------------------------

  #if TRACKING_ENABLED
    gua::math::mat4 current_cam_tracking_matrix(gua::math::mat4::identity());
    gua::math::mat4 current_pointer_tracking_matrix(gua::math::mat4::identity());
    std::thread tracking_thread([&]() {
      scm::inp::tracker::target_container targets;
      targets.insert(scm::inp::tracker::target_container::value_type(5, scm::inp::target(5)));
      targets.insert(scm::inp::tracker::target_container::value_type(17, scm::inp::target(17)));

      scm::inp::art_dtrack* dtrack(new scm::inp::art_dtrack(5000));
      if (!dtrack->initialize()) {
        std::cerr << std::endl << "Tracking System Fault" << std::endl;
        return;
      }
      while (true) {
        dtrack->update(targets);
        auto camera_target = targets.find(5)->second.transform();
        camera_target[12] /= 1000.f; camera_target[13] /= 1000.f; camera_target[14] /= 1000.f;
        auto pointer_target = targets.find(17)->second.transform();
        pointer_target[12] /= 1000.f; pointer_target[13] /= 1000.f; pointer_target[14] /= 1000.f;
        current_cam_tracking_matrix = camera_target;

        //gua::math::mat4 screen_world = screen->get_world_transform();
        current_pointer_tracking_matrix = /*scm::math::inverse(screen_world) */ gua::math::mat4(pointer_target);
        //std::cout << current_cam_tracking_matrix << "\n";
      }
    });
  #endif


    gua::math::mat4 translation_matrix = gua::math::mat4::identity();
    gua::math::mat4 current_nav_transformation = navigation->get_transform();
    auto current_translation_mat = gua::math::get_translation(current_nav_transformation);

  gua::Renderer renderer;

  // application loop  ////////////////////////////////////////////
  gua::events::MainLoop loop;
  gua::events::Ticker ticker(loop, 1.0 / 500.0);


  auto default_node_transform = plod_head->get_transform();

  unsigned passed_frames = 0;
  float i = 0; 
  double current_scaling = 1.0;
  ticker.on_tick.connect([&]() {

  #if USE_SIDE_BY_SIDE
  double amount = 1.0 / 250.0; 
  #else
  double amount = 1.0 / 65.0;
  #endif
    
  // set time variable for animation
  i += 1.0 / 600.0;
  if (i > 1) i = 0;
  character->set_time_1(i); 

    /*#if USE_QUAD_BUFFERED
    gua::math::mat4 modelmatrix =  scm::math::make_rotation(++passed_frames/90.0, 0.0, 1.0, 0.0 );
    #else*/

    // world coordinate transform of initial camera
    //make_translation(0.0,0.0, + 0.6 ) * screen->get_transform() 

    // apply trackball rotation
    #if !TRACKING_ENABLED
    gua::math::mat4 viewmatrix =   //PUT WORLD COORDINATE SYSTEM HERE
                                  gua::math::mat4(scm::math::make_translation( screen_offset_x, screen_offset_y, screen_offset_z))
                                  * gua::math::mat4(trackball.rotation()) 
                                  * gua::math::mat4(scm::math::make_translation(-screen_offset_x, -screen_offset_y, -screen_offset_z));
    #else
    /*gua::math::mat4 viewmatrix =   //PUT WORLD COORDINATE SYSTEM HERE
                                  gua::math::mat4(scm::math::make_translation(gua::math::get_translation(current_cam_tracking_matrix)))
                                  * gua::math::mat4(trackball.rotation()) 
                                  * gua::math::mat4(scm::math::make_translation((-1)*gua::math::get_translation(current_cam_tracking_matrix)));*/
    gua::math::mat4 viewmatrix =   //PUT WORLD COORDINATE SYSTEM HERE
                                  gua::math::mat4(scm::math::make_translation( screen_offset_x, screen_offset_y, screen_offset_z))
                                  * gua::math::mat4(trackball.rotation()) 
                                  * gua::math::mat4(scm::math::make_translation(-screen_offset_x, -screen_offset_y, -screen_offset_z));
    #endif

    /*gua::math::mat4 scale_mat = scm::math::make_scale(200.0, 200.0, 200.0);
     gua::math::mat4 rot_mat_x = scm::math::make_rotation(-90.0, 1.0, 0.0, 0.0);
     gua::math::mat4 rot_mat_y = scm::math::make_rotation(106.8, 0.0, 1.0, 0.0);
     gua::math::mat4 trans_mat = scm::math::make_translation(-1330.0, -15.0, -10.0);*/
     //plod_head->set_transform( trans_mat*rot_mat_y*rot_mat_x* scale_mat *  default_node_transform);


     //add simple geometry rotation
     gua::math::mat4 model_rot_matrix = scm::math::make_rotation(0.1, 0.0, 1.0, 0.0 );
     test_cube_with_trancsparency->set_transform((test_cube_with_trancsparency->get_transform())*model_rot_matrix);
                         
  gua::math::mat4 current_nav_transform = navigation->get_transform();
    
  if(reset_position){
      //navigation->set_transform(gua::math::mat4::identity());
      viewmatrix = gua::math::mat4::identity();
      translation_matrix = gua::math::mat4::identity();
      transform->set_transform(gua::math::mat4::identity());
      trackball.reset();
      reset_position = false;
  }

  auto right_vector = scm::math::normalize( gua::math::vec3(viewmatrix[0], viewmatrix[1], viewmatrix[2]) ); 
  auto up_vector = scm::math::normalize( gua::math::vec3(viewmatrix[4], viewmatrix[5], viewmatrix[6]) );
  auto forward_vector = scm::math::normalize( gua::math::vec3(-viewmatrix[8], -viewmatrix[9], -viewmatrix[10]) ); 
  auto left_vector = right_vector * (-1);
  auto down_vector = up_vector * (-1);
  auto backward_vector = forward_vector * (-1);


  //Z and X are swapped due to tracking coordinate system origin ?!
  if( moves_positive_z ) {
        //std::cout << "FWD VEC: " << right_vector << "\n";
    translation_matrix = scm::math::make_translation(right_vector * amount) * translation_matrix ;
  }

  if( moves_negative_z ){
    translation_matrix = scm::math::make_translation(left_vector * amount) * translation_matrix;
  } 

  if( moves_positive_x ) {
    translation_matrix = scm::math::make_translation(forward_vector * amount) * translation_matrix;
  }

  if( moves_negative_x ) {
    translation_matrix = scm::math::make_translation(backward_vector *amount) * translation_matrix;
  } 
    
    auto nav_transform = translation_matrix * viewmatrix;
    navigation->set_transform(nav_transform);
  //std::cout << "cam pos" << gua::math::vec3(nav_transform[12], nav_transform[13], nav_transform[14]) << "\n"; 

    #if (USE_SIDE_BY_SIDE && TRACKING_ENABLED)
    camera->set_transform(current_cam_tracking_matrix);
    light_pointer->set_transform(
                                  //gua::math::mat4(scm::math::make_translation( screen_offset_x, screen_offset_y, screen_offset_z))*
                                   current_pointer_tracking_matrix 
                                  //* gua::math::mat4(scm::math::make_translation(-screen_offset_x, -screen_offset_y, -screen_offset_z))
                                  );
    #endif 

    if (window->should_close() || close_window) {
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
