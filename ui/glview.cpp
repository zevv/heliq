
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "glview.hpp"


static GLuint compile_shader(GLenum type, const char *src)
{
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, nullptr);
	glCompileShader(s);
	GLint ok;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if(!ok) {
		char log[512];
		glGetShaderInfoLog(s, sizeof(log), nullptr, log);
		fprintf(stderr, "glview: shader compile error: %s\n", log);
		glDeleteShader(s);
		return 0;
	}
	return s;
}


static GLuint link_program(GLuint vs, GLuint fs)
{
	GLuint p = glCreateProgram();
	glAttachShader(p, vs);
	glAttachShader(p, fs);
	glBindAttribLocation(p, 0, "a_pos");
	glBindAttribLocation(p, 1, "a_color");
	glLinkProgram(p);
	GLint ok;
	glGetProgramiv(p, GL_LINK_STATUS, &ok);
	if(!ok) {
		char log[512];
		glGetProgramInfoLog(p, sizeof(log), nullptr, log);
		fprintf(stderr, "glview: program link error: %s\n", log);
		glDeleteProgram(p);
		return 0;
	}
	return p;
}


static const char *solid_vs =
"#version 100\n"
"attribute vec3 a_pos;\n"
"uniform mat4 u_mvp;\n"
"void main() {\n"
"  gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
"}\n";

static const char *solid_fs =
"#version 100\n"
"precision mediump float;\n"
"uniform vec4 u_color;\n"
"void main() {\n"
"  gl_FragColor = u_color;\n"
"}\n";

static const char *vcol_vs =
"#version 100\n"
"attribute vec3 a_pos;\n"
"attribute vec4 a_color;\n"
"uniform mat4 u_mvp;\n"
"varying vec4 v_color;\n"
"void main() {\n"
"  gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
"  v_color = a_color;\n"
"}\n";

static const char *vcol_fs =
"#version 100\n"
"precision mediump float;\n"
"varying vec4 v_color;\n"
"void main() {\n"
"  gl_FragColor = v_color;\n"
"}\n";


GLView::~GLView()
{
	if(m_sdl_tex) SDL_DestroyTexture(m_sdl_tex);
	if(m_gl_ctx) SDL_GL_DestroyContext(m_gl_ctx);
}


bool GLView::init(SDL_Renderer *rend)
{
	if(m_valid) return true;

	SDL_Window *win = SDL_GetRenderWindow(rend);
	if(!win) {
		fprintf(stderr, "glview: no window from renderer\n");
		return false;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	m_gl_ctx = SDL_GL_CreateContext(win);
	if(!m_gl_ctx) {
		fprintf(stderr, "glview: SDL_GL_CreateContext failed: %s\n", SDL_GetError());
		return false;
	}

	fprintf(stderr, "glview: GL: %s\n", glGetString(GL_RENDERER));

	if(!init_shaders()) {
		SDL_GL_DestroyContext(m_gl_ctx);
		m_gl_ctx = nullptr;
		return false;
	}

	// restore SDL renderer's context
	SDL_GL_MakeCurrent(win, nullptr);

	m_valid = true;
	fprintf(stderr, "glview: initialized\n");
	return true;
}


bool GLView::init_shaders()
{
	GLuint vs, fs;

	vs = compile_shader(GL_VERTEX_SHADER, solid_vs);
	fs = compile_shader(GL_FRAGMENT_SHADER, solid_fs);
	if(!vs || !fs) return false;
	m_prog_solid = link_program(vs, fs);
	glDeleteShader(vs);
	glDeleteShader(fs);
	if(!m_prog_solid) return false;
	m_mvp_loc = glGetUniformLocation(m_prog_solid, "u_mvp");
	m_color_loc = glGetUniformLocation(m_prog_solid, "u_color");

	vs = compile_shader(GL_VERTEX_SHADER, vcol_vs);
	fs = compile_shader(GL_FRAGMENT_SHADER, vcol_fs);
	if(!vs || !fs) return false;
	m_prog_vcol = link_program(vs, fs);
	glDeleteShader(vs);
	glDeleteShader(fs);
	if(!m_prog_vcol) return false;
	m_vcol_mvp_loc = glGetUniformLocation(m_prog_vcol, "u_mvp");

	return true;
}


void GLView::resize(int w, int h)
{
	if(w == m_w && h == m_h) return;
	m_w = w;
	m_h = h;

	// mark FBO for recreation on next begin()
	m_fbo = 0;
	m_rbo_color = 0;
	m_rbo_depth = 0;
	if(m_sdl_tex) { SDL_DestroyTexture(m_sdl_tex); m_sdl_tex = nullptr; }
	m_pixels.resize(w * h * 4);
}


void GLView::begin(SDL_Renderer *rend)
{
	if(!m_valid) return;

	SDL_Window *win = SDL_GetRenderWindow(rend);

	// save SDL renderer's GL context, switch to ours
	m_prev_ctx = SDL_GL_GetCurrentContext();
	SDL_GL_MakeCurrent(win, m_gl_ctx);

	// create FBO if needed
	if(!m_fbo && m_w > 0 && m_h > 0) {
		glGenRenderbuffers(1, &m_rbo_color);
		glBindRenderbuffer(GL_RENDERBUFFER, m_rbo_color);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8_OES, m_w, m_h);

		glGenRenderbuffers(1, &m_rbo_depth);
		glBindRenderbuffer(GL_RENDERBUFFER, m_rbo_depth);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, m_w, m_h);

		glGenFramebuffers(1, &m_fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_rbo_color);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_rbo_depth);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(status != GL_FRAMEBUFFER_COMPLETE)
			fprintf(stderr, "glview: FBO incomplete: 0x%x\n", status);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glViewport(0, 0, m_w, m_h);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


void GLView::end(SDL_Renderer *rend)
{
	if(!m_valid) return;

	SDL_Window *win = SDL_GetRenderWindow(rend);

	// read pixels from FBO
	glReadPixels(0, 0, m_w, m_h, GL_RGBA, GL_UNSIGNED_BYTE, m_pixels.data());
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// restore SDL renderer's GL context
	SDL_GL_MakeCurrent(win, m_prev_ctx);

	// GL renders bottom-up, SDL expects top-down — flip rows
	int stride = m_w * 4;
	for(int y = 0; y < m_h / 2; y++) {
		uint8_t *a = m_pixels.data() + y * stride;
		uint8_t *b = m_pixels.data() + (m_h - 1 - y) * stride;
		for(int x = 0; x < stride; x++) {
			uint8_t t = a[x]; a[x] = b[x]; b[x] = t;
		}
	}

	// create/update SDL texture
	if(!m_sdl_tex)
		m_sdl_tex = SDL_CreateTexture(rend, SDL_PIXELFORMAT_RGBA32,
		                               SDL_TEXTUREACCESS_STREAMING, m_w, m_h);
	SDL_UpdateTexture(m_sdl_tex, nullptr, m_pixels.data(), stride);
	SDL_SetTextureBlendMode(m_sdl_tex, SDL_BLENDMODE_BLEND);
}


void GLView::set_mvp(const float *mvp16)
{
	if(!m_valid) return;
	glUseProgram(m_prog_solid);
	glUniformMatrix4fv(m_mvp_loc, 1, GL_FALSE, mvp16);
	glUseProgram(m_prog_vcol);
	glUniformMatrix4fv(m_vcol_mvp_loc, 1, GL_FALSE, mvp16);
}
