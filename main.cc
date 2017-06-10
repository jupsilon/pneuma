
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <string>

#include <cstring>

#include <boost/coroutine2/all.hpp>

#include <wayland-client.h>
#include <wayland-egl.h>

#include <EGL/egl.h>
//#include <GL/gl.h>
#include <GLES3/gl3.h>

template <typename T>
using coroutine = boost::coroutines2::coroutine<T>;

template <typename T, typename D>
inline auto attach_unique(T* ptr, D deleter) {
  std::cerr << typeid (T*).name() << ':' << ptr << std::endl;
  return std::unique_ptr<T, D>(ptr, deleter);
}

using registry_bind_args = std::tuple<wl_registry*, uint32_t, char const*, uint32_t>;
inline wl_registry* registry (registry_bind_args const& args) { return std::get<0>(args); }
inline uint32_t     id       (registry_bind_args const& args) { return std::get<1>(args); }
inline char const*  interface(registry_bind_args const& args) { return std::get<2>(args); }
inline uint32_t     version  (registry_bind_args const& args) { return std::get<3>(args); }

#define CODE(x) (#x)

auto registry_bind_iter = [](wl_display* display) {
  return coroutine<registry_bind_args>::pull_type([display](auto& yield) {
      auto registry_ptr = attach_unique(wl_display_get_registry(display), wl_registry_destroy);
      wl_registry_listener listener = {
	[](void* data,
	   wl_registry* registry,
	   uint32_t id,
	   char const* interface,
	   uint32_t version)
	{
	  auto& yield = *reinterpret_cast<coroutine<registry_bind_args>::push_type*>(data);
	  yield(registry_bind_args(registry, id, interface, version));
	},
	[](void* data,
	   wl_registry* registry,
	   uint32_t id)
	{
	},
      };
      wl_registry_add_listener(registry_ptr.get(), &listener, &yield);
      wl_display_roundtrip(display);
    });
};

int main() {
  static GLfloat resolution_vec[] = { 0.0f, 0.0f, };

  try {
    auto display_ptr = attach_unique(wl_display_connect(nullptr), wl_display_disconnect);

    void* compositor_raw = nullptr;
    void* shell_raw = nullptr;
    void* seat_raw = nullptr;
    for (auto args : registry_bind_iter(display_ptr.get())) {
      if      (0 == std::strcmp(wl_compositor_interface.name, interface(args))) {
	compositor_raw = wl_registry_bind(registry(args), id(args), &wl_compositor_interface, 0);
      }
      else if (0 == std::strcmp(wl_shell_interface.name, interface(args))) {
	shell_raw = wl_registry_bind(registry(args), id(args), &wl_shell_interface, 0);
      }
      else if (0 == std::strcmp(wl_seat_interface.name, interface(args))) {
	seat_raw = wl_registry_bind(registry(args), id(args), &wl_seat_interface, 0);
      }
    }
    auto compositor_ptr = attach_unique((wl_compositor*) compositor_raw, wl_compositor_destroy);
    auto shell_ptr = attach_unique((wl_shell*) shell_raw, wl_shell_destroy);
    auto seat_ptr = attach_unique((wl_seat*) seat_raw, wl_seat_destroy);

    auto surface_ptr = attach_unique(wl_compositor_create_surface(compositor_ptr.get()),
				     wl_surface_destroy);
    auto shell_surface_ptr = attach_unique(wl_shell_get_shell_surface(shell_ptr.get(), surface_ptr.get()),
					   wl_shell_surface_destroy);

    auto egl_display_ptr = attach_unique(eglGetDisplay(display_ptr.get()), eglTerminate);
    eglInitialize(egl_display_ptr.get(), nullptr, nullptr);

    resolution_vec[0] = 1500;
    resolution_vec[1] = 1000;
    auto egl_window_ptr = attach_unique(wl_egl_window_create(surface_ptr.get(), resolution_vec[0], resolution_vec[1]),
					wl_egl_window_destroy);

    wl_shell_surface_listener listener = {
      [](void* data, wl_shell_surface* shell_surface, uint32_t serial) {
	wl_shell_surface_pong(shell_surface, serial);
      },
      [](void* data, wl_shell_surface* shell_surface, uint32_t edges, int32_t width, int32_t height) {
	wl_egl_window_resize(reinterpret_cast<wl_egl_window*>(data),
			     width, height, 0, 0);
	glViewport(0, 0, width, height);
	resolution_vec[0] = width;
	resolution_vec[1] = height;
      },
      [](void* data, wl_shell_surface* surface) {
      },
    };
    wl_shell_surface_add_listener(shell_surface_ptr.get(), &listener, egl_window_ptr.get());
    wl_shell_surface_set_toplevel(shell_surface_ptr.get());

    wl_seat_listener seat_listener = {
      [](void* data, wl_seat* seat, uint32_t caps) {
	if (caps & WL_SEAT_CAPABILITY_POINTER) {
	  std::cerr << "*** pointer device found." << std::endl;
	}
	if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
	  std::cerr << "*** keyboard device found." << std::endl;
	}
	if (caps & WL_SEAT_CAPABILITY_TOUCH) {
	  std::cerr << "*** touch device found." << std::endl;
	}
      },
    };
    wl_seat_add_listener(seat_ptr.get(), &seat_listener, nullptr);

    static GLfloat pointer_vec[2] = { -256, -256 };
    auto pointer_ptr = attach_unique(wl_seat_get_pointer(seat_ptr.get()), wl_pointer_destroy);
    wl_pointer_listener pointer_listener = {
      [](void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface, wl_fixed_t sx, wl_fixed_t sy) {
	std::cerr << "Pointer entered surface: " << sx << ',' << sy << std::endl;
      },
      [](void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface) {
	std::cerr << "Pointer left surface." << std::endl;
      },
      [](void* data, wl_pointer* pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
	std::cerr << "Pointer moved: " << sx << ',' << sy << std::endl;
	pointer_vec[0] = sx / 256.0f;
	pointer_vec[1] = resolution_vec[1] - sy / 256.0f;
      },
      [](void* data, wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
	std::cerr << "Pointer button." << std::endl;
      },
      [](void* data, wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
	std::cerr << "Pointer handle axis." << std::endl;
      }
    };
    wl_pointer_add_listener(pointer_ptr.get(), &pointer_listener, nullptr);

    eglBindAPI(EGL_OPENGL_ES_API);
    EGLint attributes[] = {
      EGL_LEVEL, 0,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_RED_SIZE,   8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE,  8,
      EGL_ALPHA_SIZE, 8,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
      EGL_NONE,
    };
    EGLConfig config;
    EGLint num_config;
    eglChooseConfig(egl_display_ptr.get(), attributes, &config, 1, &num_config);
    EGLint contextAttributes[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE,
    };
    auto egl_context_ptr = attach_unique(eglCreateContext(egl_display_ptr.get(), config, nullptr, contextAttributes),
					 [&egl_display_ptr](auto ptr) { eglDestroyContext(egl_display_ptr.get(), ptr); });

    auto egl_surface_ptr = attach_unique(eglCreateWindowSurface(egl_display_ptr.get(), config, egl_window_ptr.get(), nullptr),
					 [&egl_display_ptr](auto ptr) { eglDestroySurface(egl_display_ptr.get(), ptr); });

    eglMakeCurrent(egl_display_ptr.get(), egl_surface_ptr.get(), egl_surface_ptr.get(), egl_context_ptr.get());

    auto vid = glCreateShader(GL_VERTEX_SHADER);
    std::cerr << "***" << vid << std::endl;
    auto fid = glCreateShader(GL_FRAGMENT_SHADER);
    std::cerr << "***" << fid << std::endl;

    auto v_code =
      std::string("\n") + 
      CODE(
	   attribute vec4 position;
	   varying vec2 vert;

	   void main(void) {
	     vert = position.xy;
	     gl_Position = position;
	   }
	   );
    auto f_code =
      std::string("\n") + 
      CODE(
	   precision mediump float;
	   varying vec2 vert;
	   uniform vec2 resolution;
	   uniform vec2 pointer;

	   void main(void) {
	     // float brightness = length(gl_FragCoord.xy - resolution * (vert / 0.5 + vec2(0.5))) / length(resolution);
	     // brightness *= brightness;	     brightness *= brightness;	     brightness *= brightness;
	     // brightness = 1.0 - brightness;
	     // gl_FragColor = vec4(0.0, 0.5, brightness, brightness);
	     float brightness = length(gl_FragCoord.xy - resolution / 2.0) / length(resolution);
	     brightness = 1.0 - brightness;
	     gl_FragColor = vec4(0.0, 0.0, brightness, brightness);

	     float radius = length(pointer - gl_FragCoord.xy);
	     float touchMark = smoothstep(16.0,
					  40.0,
					  radius);

	     gl_FragColor *= touchMark;
	   }

	   );

    auto compile = [](auto id, auto code) {
      glShaderSource(id, 1, &code, nullptr);
      glCompileShader(id);
      GLint result;
      glGetShaderiv(id, GL_COMPILE_STATUS, &result);
      std::cerr << result << std::endl;
      GLint infoLogLength = 0;
      glGetShaderiv(id, GL_INFO_LOG_LENGTH, &infoLogLength);
      std::cerr << infoLogLength << std::endl;
       std::vector<char> buf(infoLogLength);
      glGetShaderInfoLog(id, infoLogLength, nullptr, &buf.front());
      std::cerr << "<<<" << std::endl;
      std::cerr << code << std::endl;
      std::cerr << "---" << std::endl;
      std::cerr << std::string(buf.begin(), buf.end()).c_str() << std::endl;
      std::cerr << ">>>" << std::endl;
    };

    compile(vid, v_code.c_str());
    compile(fid, f_code.c_str());

    auto program = glCreateProgram();
    glAttachShader(program, vid);
    glAttachShader(program, fid);

    glDeleteShader(vid);
    glDeleteShader(fid);

    glBindAttribLocation(program, 0, "position");

    glLinkProgram(program);
    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    std::cerr << linked << "(" << GL_TRUE << "/" << GL_FALSE << ")" << std::endl;
    GLint length;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::vector<char> buf(length);
    glGetProgramInfoLog(program, length, nullptr, &buf.front());
    std::cerr << "<<<" << std::endl;
    std::cerr << std::string(buf.begin(), buf.end()).c_str() << std::endl;
    std::cerr << ">>>" << std::endl;
    glUseProgram(program);

    //    auto resolution = glGetUniformLocation(program, "resolution");
    //glUniform2fv(resolution, 1, resolution_vec);

    glFrontFace(GL_CW);

    for (;;) {
      auto ret = wl_display_dispatch_pending(display_ptr.get());
      //      if (ret == -1)
      //break;
      //      if (wl_display_dispatch_pending(display_ptr.get())) {
      //}
      glClearColor(0.0, 0.8, 0.0, 0.8);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glUseProgram(program);
      GLfloat vVertices[] = {
	-1, +1, 0,
	+1, +1, 0,
	+1, -1, 0,
	-1, -1, 0,
      };
      //      glUniform2f(resolution, resolution_vec[0], resolution_vec[1]);
      glUniform2fv(glGetUniformLocation(program, "resolution"), 1, resolution_vec);
      glUniform2fv(glGetUniformLocation(program, "pointer"), 1, pointer_vec);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
      glEnableVertexAttribArray(0);
      glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

      eglSwapBuffers(egl_display_ptr.get(), egl_surface_ptr.get());
    }
  }
  catch (std::exception& ex) {
    std::cerr << ex.what() << std::endl;
  }
  catch (...) {
    std::cerr << "fatail somethings..." << std::endl;
  }
  return 0;
}
