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
#include <gua/renderer/NprOutlinePass.hpp>

#include <gua/renderer/GBuffer.hpp>
#include <gua/renderer/ABuffer.hpp>
#include <gua/renderer/Pipeline.hpp>
#include <gua/utils/Logger.hpp>

//
#include <gua/renderer/Uniform.hpp>

#include <boost/variant.hpp>

namespace gua {

  ////////////////////////////////////////////////////////////////////////////////
  NprOutlinePassDescription::NprOutlinePassDescription()
    : PipelinePassDescription()
  {
    vertex_shader_ = "shaders/common/fullscreen_quad.vert";
    fragment_shader_ = "shaders/npr_outline_halftoning.frag";
    needs_color_buffer_as_input_ = true;
    writes_only_color_buffer_ = true;
    rendermode_ = RenderMode::Quad;
    name_ = "NprOutlinePassDescription";
    depth_stencil_state_ = boost::make_optional(scm::gl::depth_stencil_state_desc(false, false));

    //defaut value
    uniforms["line_thickness"] = 1;
    //uniforms["store_for_blending"] = false;
  }

  

  ////////////////////////////////////////////////////////////////////////////////
  std::shared_ptr<PipelinePassDescription> NprOutlinePassDescription::make_copy() const {
    return std::make_shared<NprOutlinePassDescription>(*this);
  }

  ////////////////////////////////////////////////////////////////////////////////
  PipelinePass NprOutlinePassDescription::make_pass(RenderContext const& ctx, SubstitutionMap& substitution_map)
  {
    PipelinePass pass{*this, ctx, substitution_map};
    return pass;
  }

  ////////////////////////////////////////////////////////////////////////////////
  NprOutlinePassDescription& NprOutlinePassDescription::line_thickness(int value)
  {
  uniforms["line_thickness"] = value;
  return *this;
  }

  ////////////////////////////////////////////////////////////////////////////////
  NprOutlinePassDescription& NprOutlinePassDescription::halftoning(bool value)
  {
  uniforms["halftoning"] = value;
  return *this;
  }
   ////////////////////////////////////////////////////////////////////////////////
  NprOutlinePassDescription& NprOutlinePassDescription::apply_outline(bool value)
  {
  uniforms["apply_outline"] = value;
  return *this;
  }
   ////////////////////////////////////////////////////////////////////////////////
  NprOutlinePassDescription& NprOutlinePassDescription::no_color(bool value)
  {
  uniforms["no_color"] = value;
  return *this;
  }
   ////////////////////////////////////////////////////////////////////////////////
  NprOutlinePassDescription& NprOutlinePassDescription::store_for_blending(bool value)
  {
  uniforms["store_for_blending"] = value;
  return *this;
  }

}
