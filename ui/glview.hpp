#pragma once

#include <SDL3/SDL.h>
#include <vector>
#include <stdint.h>

// Offscreen GLES3 render target with 4x MSAA, outputs to an SDL_Texture.
// Uses SDL's own GL context management to avoid platform conflicts.
// Usage:
//   glview.init(window);          // once, after SDL_CreateRenderer
//   glview.resize(w, h);
//   glview.begin(window);
//   // ... GL draw calls ...
//   glview.end(window, rend);     // restores SDL context, blits to texture
//   SDL_RenderTexture(rend, glview.texture(), ...);

class GLView {
public:
	GLView() = default;
	~GLView();

	GLView(const GLView &) = delete;
	GLView &operator=(const GLView &) = delete;

	bool init(SDL_Renderer *rend);
	bool valid() const { return m_valid; }
	void resize(int w, int h);
	void begin(SDL_Renderer *rend);
	void end(SDL_Renderer *rend);
	SDL_Texture *texture() const { return m_sdl_tex; }

	// shader access
	void set_mvp(const float *mvp16);
	unsigned int solid_shader() const { return m_prog_solid; }
	int mvp_loc() const { return m_mvp_loc; }
	int color_loc() const { return m_color_loc; }
	unsigned int vcol_shader() const { return m_prog_vcol; }
	int vcol_mvp_loc() const { return m_vcol_mvp_loc; }

private:
	bool init_shaders();

	bool m_valid{false};
	int m_w{0}, m_h{0};

	SDL_GLContext m_gl_ctx{nullptr};
	SDL_GLContext m_prev_ctx{nullptr};

	// GL objects
	unsigned int m_fbo{};
	unsigned int m_rbo_color{};
	unsigned int m_rbo_depth{};
	unsigned int m_fbo_resolve{};
	unsigned int m_rbo_resolve_color{};

	// SDL texture for output
	SDL_Texture *m_sdl_tex{};
	std::vector<uint8_t> m_pixels;

	// shaders
	unsigned int m_prog_solid{};
	int m_mvp_loc{-1};
	int m_color_loc{-1};
	unsigned int m_prog_vcol{};
	int m_vcol_mvp_loc{-1};
};
