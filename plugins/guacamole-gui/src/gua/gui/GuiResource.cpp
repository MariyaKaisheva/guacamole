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
#include <gua/gui/GuiResource.hpp>

// guacamole headers
#include <gua/gui/GuiPass.hpp>
#include <gua/gui/Interface.hpp>
#include <gua/platform.hpp>
#include <gua/renderer/RenderContext.hpp>
#include <gua/utils/Logger.hpp>

// external headers
#include <Awesomium/WebCore.h>
#include <Awesomium/BitmapSurface.h>
#include <Awesomium/STLHelpers.h>

namespace gua {

namespace {

#include "AweKeyEvent.ipp"
#include "AweViewListener.ipp"
#include "AweLoadListener.ipp"
#include "AweProcessListener.ipp"
#include "AweJSMethodHandler.ipp"

}

////////////////////////////////////////////////////////////////////////////////

GuiResource::GuiResource(std::string const& url, math::vec2 const& size)
  : url_("")
  , view_(nullptr)
  , js_window_(nullptr)
  , interactive_(true)
{

  on_loaded.connect([this]() {
    js_window_ = new Awesomium::JSValue();
    *js_window_ = view_->ExecuteJavascriptWithResult(
      Awesomium::WSLit("window"), Awesomium::WSLit("")
    );

    if (!js_window_->IsObject()) {
      Logger::LOG_WARNING << "Failed to initialize GuiResource!" << std::endl;
    }
  });

  view_ = Interface::instance()->create_webview(size.x, size.y);
  view_->SetTransparent(true);
  view_->Focus();
  view_->set_view_listener(new AweViewListener());
  view_->set_load_listener(new AweLoadListener(this));
  view_->set_process_listener(new AweProcessListener());
  view_->set_js_method_handler(new AweJSMethodHandler(this));

  set_url(url);

}

////////////////////////////////////////////////////////////////////////////////

GuiResource::~GuiResource() {
  delete static_cast<AweViewListener*>(view_->view_listener());
  delete static_cast<AweLoadListener*>(view_->load_listener());
  delete static_cast<AweProcessListener*>(view_->process_listener());
  delete static_cast<AweJSMethodHandler*>(view_->js_method_handler());
  view_->Destroy();

  if (js_window_) {
    delete js_window_;
  }
}

////////////////////////////////////////////////////////////////////////////////

void GuiResource::set_url(std::string const& url) {
  url_ = url;
  Awesomium::WebURL u(Awesomium::WSLit(url_.c_str()));
  view_->LoadURL(u);
}

////////////////////////////////////////////////////////////////////////////////

std::string const& GuiResource::get_url() const {
  return url_;
}

////////////////////////////////////////////////////////////////////////////////

void GuiResource::reload() {
  view_->Reload(true);
}

////////////////////////////////////////////////////////////////////////////////

void GuiResource::focus() {
  view_->Focus();
}

////////////////////////////////////////////////////////////////////////////////

void GuiResource::set_interactive(bool interactive) {
  interactive_ = interactive;
}

////////////////////////////////////////////////////////////////////////////////

bool GuiResource::is_interactive() const {
  return interactive_;
}

////////////////////////////////////////////////////////////////////////////////

void GuiResource::inject_keyboard_event(Key key, int scancode, int action, int mods) const {
  if (interactive_) {
    view_->InjectKeyboardEvent(AweKeyEvent(key, scancode, action, mods));
  }
}

////////////////////////////////////////////////////////////////////////////////

void GuiResource::inject_char_event(unsigned c) const {
  if (interactive_) {
    view_->InjectKeyboardEvent(AweKeyEvent(c));
  }
}


////////////////////////////////////////////////////////////////////////////////

void GuiResource::inject_mouse_position(math::vec2 const& position) const {
  if (interactive_) {
    view_->InjectMouseMove(position.x, /*size.y -*/ position.y);
  }
}

////////////////////////////////////////////////////////////////////////////////

void GuiResource::inject_mouse_button(Button button, int action, int mods) const {
  if (interactive_) {
    if (action == 0) {
      view_->InjectMouseUp(static_cast<Awesomium::MouseButton>(button));
    } else {
      view_->InjectMouseDown(static_cast<Awesomium::MouseButton>(button));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void GuiResource::inject_mouse_wheel(math::vec2 const& direction) const {
  if (interactive_) {
    view_->InjectMouseWheel(direction.y, direction.x);
  }
}

////////////////////////////////////////////////////////////////////////////////

void GuiResource::call_javascript_impl(std::string const& method, std::vector<std::string> const& args) const {

  if (!js_window_) {
    return;
  }

  Awesomium::JSArray j_args;
  for (auto const& arg: args) {
    j_args.Push(Awesomium::JSValue(Awesomium::ToWebString(arg)));
  }
  js_window_->ToObject().Invoke(Awesomium::ToWebString(method), j_args);
}

////////////////////////////////////////////////////////////////////////////////

void GuiResource::add_javascript_callback(std::string const& name) {
  add_javascript_callback(name, false);
}

////////////////////////////////////////////////////////////////////////////////

void GuiResource::add_javascript_getter(std::string const& name, std::function<std::string()> callback) {
  add_javascript_callback(name, true);
  result_callbacks_[name] = callback;
}

////////////////////////////////////////////////////////////////////////////////

void GuiResource::bind(RenderContext const& ctx, ShaderProgram* program) const {
  Interface::instance()->bind(view_, ctx, program);
}

////////////////////////////////////////////////////////////////////////////////

void GuiResource::ray_test(Ray const& ray, PickResult::Options options,
                    node::Node* owner, std::set<PickResult>& hits) {

}

////////////////////////////////////////////////////////////////////////////////

void GuiResource::add_javascript_callback(std::string const& callback, bool with_result) {
  Awesomium::JSValue o = view_->ExecuteJavascriptWithResult(
    Awesomium::WSLit("guacamole"), Awesomium::WSLit("")
  );

  if (o.IsObject()) {
    o.ToObject().SetCustomMethod(Awesomium::ToWebString(callback), with_result);
  }
}

////////////////////////////////////////////////////////////////////////////////

}
