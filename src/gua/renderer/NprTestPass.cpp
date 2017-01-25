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

// class header
#include <gua/renderer/NprTestPass.hpp>

#include <gua/renderer/GBuffer.hpp>
#include <gua/renderer/ABuffer.hpp>
#include <gua/renderer/Pipeline.hpp>
#include <gua/utils/Logger.hpp>

//
#include <gua/renderer/Uniform.hpp>

#include <boost/variant.hpp>

namespace gua {

  ////////////////////////////////////////////////////////////////////////////////
  NprTestPassDescription::NprTestPassDescription()
    : PipelinePassDescription()
  {
    vertex_shader_ = "shaders/common/fullscreen_quad.vert";
    fragment_shader_ = "shaders/npr_test.frag";
    needs_color_buffer_as_input_ = true;
    writes_only_color_buffer_ = true;
    rendermode_ = RenderMode::Quad;
    name_ = "NprTestPassDescription";
    depth_stencil_state_ = boost::make_optional(scm::gl::depth_stencil_state_desc(false, false));

  }

  

  ////////////////////////////////////////////////////////////////////////////////
  std::shared_ptr<PipelinePassDescription> NprTestPassDescription::make_copy() const {
    return std::make_shared<NprTestPassDescription>(*this);
  }

  ////////////////////////////////////////////////////////////////////////////////
  PipelinePass NprTestPassDescription::make_pass(RenderContext const& ctx, SubstitutionMap& substitution_map)
  {
    PipelinePass pass{*this, ctx, substitution_map};
    return pass;
  }

  ////////////////////////////////////////////////////////////////////////////////
  NprTestPassDescription& NprTestPassDescription::sphere_radius(float value) 
  {
  uniforms["sphere_radius"] = value;
  return *this;
  }
   ////////////////////////////////////////////////////////////////////////////////
  NprTestPassDescription& NprTestPassDescription::sphere_location_x(float value)
  {
  uniforms["sphere_location_x"] = value;
  return *this;
  }
     ////////////////////////////////////////////////////////////////////////////////
  NprTestPassDescription& NprTestPassDescription::sphere_location_y(float value)
  {
  uniforms["sphere_location_y"] = value;
  return *this;
  }
     ////////////////////////////////////////////////////////////////////////////////
  NprTestPassDescription& NprTestPassDescription::sphere_location_z(float value)
  {
  uniforms["sphere_location_z"] = value;
  return *this;
  }

}
