#pragma once

#include <string>
#include <vector>
#include <variant>
#include "grid.hpp"
#include "configspace.hpp"
#include "setup.hpp"

// --- Commands (UI → Sim) ---

struct CmdAdvance      { double wall_dt; };
struct CmdSingleStep   {};
struct CmdSetDt        { double dt; };
struct CmdSetTimescale { double ts; };
struct CmdSetRunning   { bool run; };
struct CmdMeasure      { int axis; };
struct CmdDecohere     { int axis; };
struct CmdSetAbsorb    { bool on; float width; float strength; };
struct CmdLoad         { Setup setup; };

using SimCommand = std::variant<
	CmdAdvance, CmdSingleStep,
	CmdSetDt, CmdSetTimescale, CmdSetRunning,
	CmdMeasure, CmdDecohere, CmdSetAbsorb,
	CmdLoad
>;

// --- Extraction requests (UI → Sim, declarative) ---

struct ExtractionRequest {
	int axes[MAX_RANK];    // free axes, -1 terminated
	int cursor[MAX_RANK];  // position on fixed axes (ignored for marginal)
	bool marginal{};       // false=slice, true=marginal

	bool operator==(const ExtractionRequest &o) const {
		if(marginal != o.marginal) return false;
		for(int i = 0; i < MAX_RANK; i++) {
			if(axes[i] != o.axes[i]) return false;
			if(axes[i] == -1) break;
		}
		if(!marginal) {
			for(int i = 0; i < MAX_RANK; i++)
				if(cursor[i] != o.cursor[i]) return false;
		}
		return true;
	}
};

struct ExtractionSet {
	static constexpr int MAX_REQUESTS = 8;
	ExtractionRequest req[MAX_REQUESTS];
	int count{};

	// returns slot index, or -1 if full
	int find_or_insert(const ExtractionRequest &r) {
		for(int i = 0; i < count; i++)
			if(req[i] == r) return i;
		if(count >= MAX_REQUESTS) return -1;
		req[count] = r;
		return count++;
	}
};

// --- Extraction results (Sim → UI) ---
// All fields always populated for every request.
// For slices: psi/pot are complex values at cursor; coherent is empty.
// For marginals: psi holds |ψ|² (real), pot holds |ψ|²-weighted potential
//   (real), coherent holds ∫ψ (complex sum over hidden axes).

struct ExtractionResult {
	int axes[MAX_RANK]{};    // mirrors request
	bool marginal{};
	int shape[MAX_RANK]{};   // points per extracted axis, -1 terminated
	std::vector<psi_t> psi;      // slice: complex psi; marginal: |ψ|² (im=0)
	std::vector<psi_t> pot;      // slice: complex V; marginal: |ψ|²-weighted V
	std::vector<psi_t> coherent; // marginal only: ∫ψ over hidden axes (complex)
};

// --- Grid metadata snapshot ---

struct GridMeta {
	int rank{};
	Axis axes[MAX_RANK]{};
	ConfigSpace cs{};
};

// --- Published state (Sim → UI) ---
// Always published every cycle, regardless of extraction requests.

struct PublishedState {
	// extraction results (per-request)
	ExtractionResult results[ExtractionSet::MAX_REQUESTS];
	int n_results{};

	// scalars (always published)
	double sim_time{};
	size_t step_count{};
	double total_probability{1.0};
	double phase_v{};          // max potential phase per step
	double phase_k{};          // max kinetic phase per step
	double dt{};               // current timestep
	double timescale{};        // sim-time per wall-second
	double k_nyquist_ratio[MAX_RANK]{};
	int marginal_peaks[MAX_RANK]{}; // argmax of |ψ|² marginal per axis

	// grid + configspace metadata
	GridMeta grid{};

	// setup metadata
	std::string title;
	std::string description;
	int n_particles{};

	// lifecycle
	int generation{};          // incremented on load/reload
	bool running{};
	std::string error;         // non-empty = fatal/blocking error
};
