#include <imgui.h>
#include <SDL3/SDL.h>
#include <math.h>
#include <string>

#include "widget.hpp"
#include "widgetregistry.hpp"
#include "experiment.hpp"
#include "config.hpp"

// color palettes: value [0..1] -> RGBA
// alpha carries intensity, color is always full brightness
static uint32_t palette_flame(double v, double gamma) {
	double t = fmin(1.0, v);
	int a = (int)(255 * pow(t, gamma));
	int r = 255;
	int g = (int)(255 * fmin(1.0, t * 2.0));
	int b = (int)(255 * fmin(1.0, fmax(0.0, t * 2.0 - 1.0)));
	return (a << 24) | (b << 16) | (g << 8) | r;
}

static uint32_t palette_gray(double v, double gamma) {
	double t = fmin(1.0, v);
	int a = (int)(255 * pow(t, gamma));
	return (a << 24) | (255 << 16) | (255 << 8) | 255;
}

enum class DataSource {
	PsiSq,     // |ψ|²
	PsiRe,     // Re(ψ)
	PsiIm,     // Im(ψ)
	PsiPhase,  // arg(ψ)
	Potential,  // Re(V)
	COUNT
};

static const char *datasource_names[] = {
	"|psi|^2", "Re(psi)", "Im(psi)", "phase(psi)", "potential"
};

enum class Palette {
	Flame,
	Gray,
	COUNT
};

static const char *palette_names[] = {
	"flame", "gray"
};

struct Overlay {
	DataSource source{DataSource::PsiSq};
	Palette palette{Palette::Flame};
	float opacity{1.0f};
	float gamma{0.5f};  // alpha exponent: 1.0=linear, 0.5=sqrt, 0.3=aggressive
	SDL_Texture *tex{};
	int tex_w{};
	int tex_h{};
};

class WidgetGrid : public Widget {
public:
	WidgetGrid(Widget::Info &info) : Widget(info) {
		// default: overlay 0 = potential in gray, overlay 1 = |ψ|² in flame
		m_overlays[0].source = DataSource::Potential;
		m_overlays[0].palette = Palette::Gray;
		m_overlays[0].opacity = 0.6f;
		m_overlays[1].source = DataSource::PsiSq;
		m_overlays[1].palette = Palette::Flame;
		m_overlays[1].opacity = 1.0f;
	}

	~WidgetGrid() {
		for(auto &ov : m_overlays)
			if(ov.tex) SDL_DestroyTexture(ov.tex);
	}

	void do_save(ConfigWriter &cfg) override {
		for(int i = 0; i < N_OVERLAYS; i++) {
			cfg.push(i);
			cfg.write("source", (int)m_overlays[i].source);
			cfg.write("palette", (int)m_overlays[i].palette);
			cfg.write("opacity", m_overlays[i].opacity);
			cfg.write("gamma", m_overlays[i].gamma);
			cfg.pop();
		}
	}

	void do_load(ConfigReader::Node *node) override {
		if(!node) return;
		for(int i = 0; i < N_OVERLAYS; i++) {
			auto *n = node->find(std::to_string(i).c_str());
			if(!n) continue;
			int src = (int)m_overlays[i].source;
			int pal = (int)m_overlays[i].palette;
			n->read("source", src);
			n->read("palette", pal);
			n->read("opacity", m_overlays[i].opacity);
			n->read("gamma", m_overlays[i].gamma);
			m_overlays[i].source = (DataSource)src;
			m_overlays[i].palette = (Palette)pal;
		}
	}

	void do_draw(Experiment &exp, SDL_Renderer *rend, SDL_Rect &r) override {
		SDL_SetRenderDrawColor(rend, 10, 10, 15, 255);
		SDL_RenderFillRect(rend, nullptr);

		// controls at the top
		draw_controls();
		float ctrl_h = ImGui::GetCursorPosY();

		if(exp.simulations.empty()) {
			ImGui::Text("No simulation");
			return;
		}

		auto &sim = *exp.simulations[0];
		auto &grid = sim.grid;

		// determine texture dimensions from grid
		int tw = (grid.rank >= 1) ? grid.axes[0].points : 1;
		int th = (grid.rank >= 2) ? grid.axes[1].points : 1;

		// render textures below controls
		SDL_FRect dst = {
			(float)r.x,
			(float)r.y + ctrl_h,
			(float)r.w,
			(float)r.h - ctrl_h
		};

		for(int oi = 0; oi < N_OVERLAYS; oi++) {
			auto &ov = m_overlays[oi];
			ensure_texture(rend, ov, tw, th);
			fill_texture(ov, sim, tw, th);

			SDL_SetTextureAlphaMod(ov.tex, (uint8_t)(ov.opacity * 255));
			SDL_SetTextureBlendMode(ov.tex, SDL_BLENDMODE_BLEND);
			SDL_RenderTexture(rend, ov.tex, nullptr, &dst);
		}
	}

private:
	static constexpr int N_OVERLAYS = 2;
	Overlay m_overlays[N_OVERLAYS]{};

	void ensure_texture(SDL_Renderer *rend, Overlay &ov, int w, int h) {
		if(ov.tex && ov.tex_w == w && ov.tex_h == h) return;
		if(ov.tex) SDL_DestroyTexture(ov.tex);
		ov.tex = SDL_CreateTexture(rend, SDL_PIXELFORMAT_RGBA32,
			SDL_TEXTUREACCESS_STREAMING, w, h);
		ov.tex_w = w;
		ov.tex_h = h;
	}

	void fill_texture(Overlay &ov, Simulation &sim, int tw, int th) {
		void *pixels;
		int pitch;
		if(!SDL_LockTexture(ov.tex, nullptr, &pixels, &pitch)) return;

		auto *psi = sim.psi_front();
		auto *pot = sim.potential;
		size_t total = sim.grid.total_points();

		// find data range for normalization
		double vmin = 0, vmax = 1e-30;
		for(size_t i = 0; i < total; i++) {
			double v = sample_value(ov.source, psi[i], pot[i]);
			if(v < vmin) vmin = v;
			if(v > vmax) vmax = v;
		}
		double range = vmax - vmin;
		if(range < 1e-30) range = 1.0;

		for(int y = 0; y < th; y++) {
			uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
			for(int x = 0; x < tw; x++) {
				size_t idx = (size_t)(th - 1 - y) * tw + x;
				if(idx >= total) idx = 0;
				double v = sample_value(ov.source, psi[idx], pot[idx]);
				double norm = (v - vmin) / range;

				switch(ov.palette) {
					case Palette::Flame: row[x] = palette_flame(norm, ov.gamma); break;
					case Palette::Gray:  row[x] = palette_gray(norm, ov.gamma); break;
					default: row[x] = 0; break;
				}
			}
		}

		SDL_UnlockTexture(ov.tex);
	}

	double sample_value(DataSource src, std::complex<double> psi, std::complex<double> pot) {
		switch(src) {
			case DataSource::PsiSq:    return std::norm(psi);
			case DataSource::PsiRe:    return psi.real();
			case DataSource::PsiIm:    return psi.imag();
			case DataSource::PsiPhase: return std::arg(psi);
			case DataSource::Potential: return pot.real();
			default: return 0;
		}
	}

	void draw_controls() {
		for(int i = 0; i < N_OVERLAYS; i++) {
			auto &ov = m_overlays[i];
			ImGui::PushID(i);
			ImGui::Text("Overlay %d:", i);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(100);
			int src = (int)ov.source;
			if(ImGui::Combo("##src", &src, datasource_names, (int)DataSource::COUNT))
				ov.source = (DataSource)src;
			ImGui::SameLine();
			ImGui::SetNextItemWidth(80);
			int pal = (int)ov.palette;
			if(ImGui::Combo("##pal", &pal, palette_names, (int)Palette::COUNT))
				ov.palette = (Palette)pal;
			ImGui::SameLine();
			ImGui::SetNextItemWidth(60);
			ImGui::SliderFloat("##alpha", &ov.opacity, 0.0f, 1.0f, "a%.1f");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(60);
			ImGui::SliderFloat("##gamma", &ov.gamma, 0.1f, 2.0f, "g%.1f");
			ImGui::PopID();
		}
	}
};

REGISTER_WIDGET(WidgetGrid,
	.name = "grid",
	.description = "Grid heatmap viewer",
	.hotkey = ImGuiKey_F3,
)
