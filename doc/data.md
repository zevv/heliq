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

- FR-005: Wavefunction data is a flat contiguous array of
  std::complex<double>, allocated via fftw_malloc (alignment matters for
  SIMD). Size is product of all axis dimensions. Accessed via precomputed
  strides.

## To Be Decided

- TBD-001: ~~Intermediate representation between Lua and C++.~~ Resolved →
  DEC-002. The Lua table *is* the intermediate representation. C++ walks
  the validated table directly. No C++ struct zoo for potential types.
  Simulation setup code reads from the table to sample potentials and
  initial wavefunctions onto its grid.

- TBD-002: ~~Potential representation.~~ Resolved → DEC-003.

- TBD-003: Double buffer swap mechanism. Atomic pointer swap, or something
  more structured? The solver thread writes to the back buffer, then swaps.
  The UI thread reads the front buffer. Need to ensure the UI never reads
  a partially-written buffer. A single atomic<int> ping-ponging between 0
  and 1 is probably sufficient.

- TBD-004: ~~Where do physical constants live?~~ Resolved → DEC-004.

- TBD-005: Keep lua_State alive after experiment setup? Discarding it is
  clean — Lua is just a config parser. Keeping it allows: live tweaking of
  parameters, time-dependent potentials evaluated in Lua, hot-reload of
  experiment scripts. Not v1 but worth not precluding.

## Decisions

- DEC-002: Lua is embedded (liblua linked, lua_State in-process). Pipeline:
  user script runs and returns a table → our Lua transform/validation
  scripts process that table (sanity checks, unit conversions, expanding
  shorthand, computing derived values) → C++ traverses the final clean
  table. All in one lua_State. Lua state may be discarded after ingestion
  or kept alive (TBD-005). We ship a small Lua library of validation/transform
  functions alongside the binary.

- DEC-001: C++23. `unique_ptr` for the ownership graph (Experiment→Simulation,
  Simulation→buffers) and anywhere error paths could leak. Raw pointers for
  non-owning references (widget receives Experiment&). No `shared_ptr` — if
  ownership is ambiguous, fix the design. `fftw_malloc`/`fftw_free` for
  wavefunction and potential arrays (SIMD alignment). Resolves nothing,
  constrains everything.

- DEC-003: Potential is complex<double>* from the start, same shape as the
  grid. Real part is the physical potential (barriers, wells). Imaginary
  part is the absorbing boundary layer (zero in the interior). Spin-matrix
  potential is a future concern, refactor when needed.

- DEC-004: Physical constants are constexpr in a header (constants.hpp).
  Universal constants only: hbar, eV, elementary_charge. Particle-specific
  properties (electron mass, proton mass, etc.) live in the Lua primitive
  library as part of the species definitions — electron() knows its own
  mass. C++ reads mass from the Lua table per-particle, doesn't hardcode
  species. Constants also exported into the Lua environment so scripts
  can use them.

## Actionable Items

- ACT-001: Define Grid struct and axis descriptors.
- ACT-002: Define FFT backend interface (Plan, forward, inverse).
- ACT-003: Define Simulation class — members, lifetime, interface to widgets.
- ACT-004: Define Experiment class — owns Simulations, holds definition, clock.
- ACT-005: Physical constants header.
- ACT-006: Rename Simulation& → Experiment& in the panel/widget call chain
  (currently simulation.hpp is an empty placeholder).

## Done

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
- fftw_malloc required for SIMD alignment — fftw_plan_dft will use SSE/AVX
  if memory is 16/32-byte aligned. Regular new/malloc may not be.
- FFTW_MEASURE at plan creation time — takes seconds but produces faster
  plans than FFTW_ESTIMATE. Run once at Simulation setup.
- the Lua→C++ boundary: Lua scripts produce a description, C++ consumes it.
  The description is pure data — no Lua state kept after experiment setup.
  Or do we want live Lua for time-dependent potentials later?
- Grid does not own data — it's a descriptor. Multiple buffers (front/back,
  potential, initial state snapshot) share the same Grid descriptor.
