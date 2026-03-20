# Data Structures

## Instructions

**IMMUTABLE. This section MUST be preserved verbatim in every design
document, every version. Do NOT edit, summarize, reword, or omit.**

Living document. Born at conception, lives with the project. Early versions
will be wrong. That is expected. The document converges on truth over time.

### Sections

- **Rationale** — the problem, stakeholders, forces. No solutions.
- **FR-NNN** — what the system must do. Amend in place, mark `(revised)`.
- **TBD-NNN** — open questions = preserved decision space. May linger.
- **DEC-NNN** — decisions made. TBD-N becomes DEC-N (same number).
- **ACT-NNN** — implementation tasks. Move to Done when finished.
- **Done** — one-liner per completed ACT.
- **Scratch** — raw dump. User or agent, anytime. Process periodically:
  promote to FR/TBD/ACT or discard.

### Process

The user starts with prose, ramblings, half-formed ideas. That is the input.
The agent's job is to extract structure from this — FRs, TBDs, DECs — by
**interviewing the user. One question at a time.** Do not batch questions.
Do not assume answers. The interview continues throughout the life of the
document, not just at creation.

### Rules

1. **NEVER renumber.** Numbers are identifiers, not order. Renumbering
   breaks every cross-reference. Do not do it.
2. **BEFORE EVERY DEC: scan the entire TBD list.** Does this decision
   narrow or foreclose ANY open TBD? If yes, you MUST state it in the DEC.
   This is not optional. This is the most important rule in this document.
   Unknown unknowns exist — this will not catch everything. Do it anyway.
3. **When an FR changes: scan ALL downstream DECs, TBDs, ACTs for impact.**
   Do NOT proceed until impacts are assessed and flagged.
4. No 1:1 mapping between sections. Cross-reference freely.
5. **TBDs linger** until they block the critical path. Do NOT resolve
   TBDs early to tidy the document.
6. History is in git. Do NOT version items inside the document.
7. **Build for now.** Not for hypothetical futures. Requirements WILL
   change.

---

## Rationale

C++ data structures for the quantum simulator. Translates the conceptual
model (doc/model.md) into concrete types. Key constraints from the model:

- No singletons — everything instantiable N times (model DEC-003, FR-010)
- SI units throughout (model DEC-005)
- Grid is rank-agnostic, flat contiguous memory (model DEC-002)
- FFT backend is opaque (model FR-008, DEC-006)
- Solver thread decoupled from UI, double-buffered (model DEC-004)
- Experiments defined via Lua scripts (model DEC-008)
- Architecture must not preclude spin (model TBD-008): axis descriptors
  carry spatial/internal flag

## Functional Requirements

- FR-001: An Experiment is a self-contained top-level object. It holds the
  world definition (domain, particles, potentials), owns N Simulation
  instances, manages the global clock and playback timescale. App holds
  N Experiments (typically 1). No global state.

- FR-002: A Grid describes an N-dimensional rectangular domain. Per-axis:
  point count, physical extent (min/max in meters), spacing (derived),
  spatial/internal flag. Max rank 8, fixed-size axis arrays. The Grid is
  a descriptor — it does not own wavefunction data.

- FR-003: A Simulation owns the running state for one solver instance: the
  wavefunction data (double-buffered), the sampled potential, the FFT plan
  handle, dt, step count, its Grid descriptor. Created from an Experiment
  definition with a specific model configuration (resolution, factoring
  strategy, dt).

- FR-004: The FFT backend is an opaque interface. A Plan handle is created
  from grid rank and dimensions, used for forward/inverse transforms.
  The FFTW implementation owns fftw_plan internally. The solver never
  sees FFTW types.

- FR-005: (revised) Wavefunction data is a flat contiguous array of
  psi_t (std::complex<float>), stored as std::vector<psi_t>. Size is
  product of all axis dimensions. Grid strides are private — all access
  goes through axis_view()/slice_view() to prevent raw indexing bugs.

## To Be Decided

- TBD-001: ~~Intermediate representation between Lua and C++.~~ Resolved →
  DEC-002. Lua script produces a table, prelude.lua validates/transforms
  it, loader.cpp converts to Setup struct (pure C++ data). Setup is the IR.
  Simulation reads from Setup, never from Lua state.

- TBD-002: ~~Potential representation.~~ Resolved → DEC-003.

- TBD-003: ~~Double buffer swap mechanism.~~ Resolved: triple-buffered
  PublishedState (model/triplebuf.hpp). Writer never blocks, reader
  always gets latest. See doc/simthread.md.

- TBD-004: ~~Where do physical constants live?~~ Resolved → DEC-004.

- TBD-005: Keep lua_State alive after experiment setup? Discarding it is
  clean — Lua is just a config parser. Keeping it allows: live tweaking of
  parameters, time-dependent potentials evaluated in Lua, hot-reload of
  experiment scripts. Not v1 but worth not precluding.

## Decisions

- DEC-002: (revised) Lua is embedded (liblua linked, lua_State in-process).
  Pipeline: user script runs → prelude.lua validates, transforms, computes
  defaults (dt, timescale, Nyquist checks) → loader.cpp traverses the Lua
  table and builds a Setup struct (pure C++ data, setup.hpp). Lua state
  discarded after ingestion. Setup is the single IR between Lua and model.
  Keeping Lua alive is deferred (TBD-005).

- DEC-001: (revised) C++23. `unique_ptr` for the ownership graph
  (Experiment→Simulation, Simulation→Solver). No `shared_ptr`.
  Wavefunction/potential arrays are std::vector<psi_t> — no fftw_malloc
  (GPU backend manages its own device buffers). Widgets receive
  SimContext&, not raw Experiment/Simulation references.

- DEC-003: (revised) Potential is std::vector<psi_t> (complex<float>),
  same shape as the grid. Real part is the physical potential (barriers,
  wells). Imaginary part is the absorbing boundary layer (zero in the
  interior). Spin-matrix potential is a future concern.

- DEC-004: Physical constants are constexpr in a header (constants.hpp).
  Universal constants only: hbar, eV. Particle-specific
  properties (electron mass, proton mass, etc.) live in the Lua primitive
  library as part of the species definitions — electron() knows its own
  mass. C++ reads mass from the Lua table per-particle, doesn't hardcode
  species. Constants also exported into the Lua environment so scripts
  can use them.

## Actionable Items

(all current items done — see Done section)

## Done

- ACT-001: Grid struct (grid.hpp) — N-dimensional, private strides, axis_view/slice_view accessors.
- ACT-002: Solver base class (solver.hpp) — virtual step/init, CPU and GPU backends.
- ACT-003: Simulation class (simulation.hpp) — owns psi, potential, solver, grid, configspace.
- ACT-004: Experiment class (experiment.hpp) — owns N simulations, advance loop, timescale.
- ACT-005: Physical constants (constants.hpp) — hbar, eV, elementary_charge as constexpr.
- ACT-006: Widget interface uses SimContext, not Simulation/Experiment references.

## Scratch

Agent note: this section is a dumping ground for raw thoughts, ideas, and
half-formed requirements. The agent will periodically review, consolidate
into FR/TBD/ACT entries, and confer with the user when needed.

- Lua primitive library: ship functions like electron(), barrier(), slit(),
  harmonic_well(), domain() etc. These provide sensible defaults (electron
  mass, charge) so user scripts are minimal. User only specifies what
  differs from defaults. Our validation/transform pass fills in derived
  values and catches errors. The library is just Lua — user can read it,
  override it, extend it.
- FFTW wisdom saved/loaded from ~/.cache/heliq/fftwf_wisdom. FFTW_MEASURE
  on first run, fast on subsequent.
- GPU solver uses VkFFT + OpenCL. CPU fallback uses fftwf. Both float32.
- Lua→C++ boundary: script → prelude.lua validation → loader.cpp → Setup
  struct. Lua state discarded after ingestion. Time-dependent potentials
  would require keeping it alive (model TBD-005 in data.md).
- Grid does not own data — it's a descriptor. Simulation owns psi and
  potential vectors. ConfigSpace maps particles to grid axes.
