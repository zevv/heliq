# Simulation Model

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

Numerical Schrödinger equation solver with real-time interactive visualization.
No toy abstractions — real math on a discretized grid, split-step Fourier
method. The solver operates on an N-dimensional complex wavefunction where N
is the product of particle count and spatial dimensions (1 particle in 2D = 2D
grid, 2 particles in 1D = 2D grid, 2 particles in 2D = 4D grid). Multiple
simultaneous views (marginals, conditional slices, phase space, etc.) observe
the same evolving state. Human-consumption frame rates; CPU-bound is acceptable.

Stakeholder: the user. Not a physicist — a technically proficient developer
with DSP experience and linear algebra basics, learning QM by building tools
that simulate it. Background reading: Sean Carroll (Something Deeply Hidden),
Tim Maudlin (Philosophy of Physics: Quantum Theory), David Albert (Quantum
Mechanics and Experience) — all philosophically inclined, focused on
interpretation and what the formalism *means*, not just how to calculate.
The user wants to develop physical intuition through direct manipulation:
set up real experiments with real units, adjust parameters, see what happens,
see what breaks. The simulation must be transparent and inspectable, never
a black box.

## Functional Requirements

- FR-001: Evolve a complex wavefunction ψ on an N-dimensional regular grid
  using split-step Fourier method. The grid rank N and resolution per axis are
  configurable at setup time, not at runtime.

- FR-002: Arbitrary potential V defined as a function of grid coordinates.
  The potential is evaluated once at setup and stored on the grid. Time-dependent
  potentials are a future concern.

- FR-003: The simulation runs as a loop: each step applies the split-step
  algorithm (half-step potential, full-step kinetic via FFT, half-step potential).
  Step size dt is configurable.

- FR-004: Multiple widgets can observe the simulation state simultaneously.
  The simulation produces one authoritative state; widgets read it, never write it.

- FR-005: The simulation can be paused, single-stepped, and resumed. Playback
  speed (world time scale) is adjustable at any time: slow, normal, fast,
  max speed. Transport controls: play, pause, step, fast-forward, reset
  (to t=0 from stored initial state). Reverse evolution (negate dt) is
  available as a separate mode — time-reversal is exact in the math but
  lossy in floating point, and watching that degradation is instructive.

- FR-008: The FFT interface must be opaque — the solver calls through an
  abstraction that hides whether FFTW, cuFFT, or something else is behind it.
  This allows swapping in GPU-accelerated transforms later without touching
  the solver logic.

- FR-006: The wavefunction preserves unitarity (total probability = 1) across
  time steps within numerical precision. This is a correctness invariant, not
  a feature — violations indicate bugs.

- FR-007: The simulation is inspectable. Show dt, step count, current
  simulation time, total probability (unitarity check), grid dimensions,
  spatial extent, energy if computable. All quantities labeled in real
  physical units with names that connect to the physics. The user learns
  by adjusting knobs and seeing what breaks — the UI must support this.

- FR-015: For 1D wavefunctions, provide a 3D helix visualization: position
  on the x-axis, Re(ψ) on y, Im(ψ) on z, with the tips connected as a
  continuous curve. This is the complete, lossless representation — a plane
  wave appears as a helix, a wave packet as a helix with a Gaussian envelope,
  momentum shows as pitch. The view must be rotatable so the user can look
  along different axes (looking down z collapses to Re(ψ) vs x, looking
  down y collapses to Im(ψ) vs x, looking along x shows the complex plane).
  This is a core visualization, not optional.

- FR-009: An experiment is defined by a "world": a spatial domain (N-dimensional
  grid with per-axis resolution and physical extent), a set of particles
  (count and mass), a potential V over configuration space, and initial
  wavefunctions. This is the complete specification — everything needed to
  run a simulation.

- FR-010: An experiment can spawn N simulation instances, each with its own
  model configuration: grid resolution, factoring strategy, dt, etc. All
  originate from the same experiment definition (same physics, same initial
  conditions). The simple case is one experiment, one simulation. The power
  case is one experiment, multiple simulations with different parameters,
  running side by side for comparison.

- FR-013: Each simulation instance has its own fixed dt. Different instances
  from the same experiment can run at different time steps. All simulations
  are kept in lockstep: the experiment maintains a global simulation time,
  and each instance advances to that time before the next frame is rendered.
  A sim with dt=0.1fs takes twice as many steps per frame as one with dt=0.2fs
  to reach the same physical moment. At max speed, the slowest simulation
  dictates the pace — the others wait. This enables direct comparison of
  numerical accuracy: same physics, same moment, different dt.

- FR-014: The experiment has a playback timescale: a user-adjustable ratio
  of simulation time to wall-clock time (e.g. 1 fs per second). The user
  can speed up, slow down, or set to max speed. At max speed, the slowest
  simulation dictates the pace. The timescale is a UI control, not a physics
  parameter — it does not affect dt or simulation accuracy.

- FR-012: Widgets have access to all simulation instances from the experiment.
  A widget chooses what to render: a single simulation, an overlay comparing
  two, a divergence metric across several. The widget is the consumer, the
  experiment is the provider. No 1:1 binding between widgets and simulations.

- FR-011: Support a factored (product state) simulation mode where each
  particle is evolved independently with only its own external potential,
  ignoring inter-particle interactions. This is the "pretend they're separate"
  approximation. Running this alongside the full joint simulation (FR-010)
  and comparing the results shows the user when entanglement matters and
  when it doesn't. A divergence metric (e.g. overlap between the factored
  product and the full joint state) quantifies this.

## To Be Decided

- TBD-001: ~~Threading model.~~ Resolved → DEC-004.

- TBD-002: ~~How is the initial wavefunction specified?~~ Resolved by
  DEC-008: Gaussian wave packets in Lua scripts (position, momentum, width).

- TBD-008: Spin. Each grid point could carry a small vector of 2s+1 complex
  components instead of a scalar. Spin-½ = 2 components, spin-1 = 3, etc.
  The FFT runs over spatial dimensions only; the spin axis is carried along.
  The potential becomes a (2s+1)×(2s+1) matrix per grid point. Not needed
  for v1 (all spatial experiments work without it) but the grid data layout
  should not preclude it. The axis descriptor needs a flag: spatial (participates
  in FFT) vs internal (does not).

- TBD-009: Magnetic fields. Required for Stern-Gerlach (stretch goal). Enters
  the Hamiltonian in two ways: (1) vector potential **A** modifies the kinetic
  term from p²/2m to (p-eA)²/2m — shifts the momentum-space multiplication
  in the FFT step, (2) Pauli spin-field coupling μ·**B** — a 2×2 matrix term
  in the potential step. Both compatible with split-step, but require spin
  (TBD-008) and a richer potential representation. Not v1.

- TBD-003: ~~Boundary conditions.~~ Resolved → DEC-009.

- TBD-010: ~~3D rendering approach.~~ Resolved: GLES3 with 4x MSAA via
  EGL/SDL. Custom mat4/vec3 math (math3d.hpp), camera with orbit/pan/zoom
  (camera3d.hpp). No external 3D library.

- TBD-004: ~~What physical units~~ Resolved → DEC-005.

- TBD-005: ~~How does the Simulation expose state to widgets?~~ Resolved:
  SimContext extraction pipeline. Widgets declare ExtractionRequests,
  model computes slices/marginals, results published via triple buffer.
  Widgets never see raw psi. See doc/simthread.md.

- TBD-006: ~~FFTW plan management.~~ Resolved → DEC-006.

- TBD-007: ~~Memory layout for multi-particle systems.~~ Resolved by DEC-002
  and DEC-010: flat contiguous array of complex<float> with precomputed
  strides. Per-axis descriptor carries size, physical extent, and label.
  Max rank capped at 8 (fixed arrays, no heap allocation for axis metadata).

## Decisions

- DEC-007: Scope is non-relativistic quantum mechanics. Massive particles
  (electrons etc.) moving through potentials. No QFT, no QED, no photons
  as particles. Static electric fields are just potentials. Magnetic fields
  and spin are stretch goals (TBD-008, TBD-009) — the architecture must not
  preclude them, but v1 is scalar wavefunctions in scalar potentials.

- DEC-008: Experiments are defined via Lua scripts. The user writes small
  scripts that compose an experiment from primitives: define particles
  (species, position, momentum, width), define potentials (barriers, wells,
  slits — geometric primitives in real SI units), define the spatial domain
  (extent, resolution). No GUI experiment editor. The script is the
  source of truth for the experiment definition. Narrows TBD-002: initial
  wavefunctions are specified as Gaussian wave packets in the Lua script
  (position, momentum, width). More complex initial states can be added later.

- DEC-001: (revised) C++23 / SDL3 / Dear ImGui / GLES3. GPU solver via
  VkFFT + OpenCL (primary), CPU fallback via FFTW float (fftwf).
  Float32 throughout — psi_t = complex<float>. GPU operates in float32
  natively, no conversion. Half the memory, adequate precision for
  visualization. See DEC-010.

- DEC-002: The solver is grid-rank-agnostic. It operates on an N-dimensional
  complex array. FFTW handles arbitrary-rank transforms. The interpretation
  of axes (which belong to which particle, which are spatial) lives outside
  the solver. Narrows TBD-007: the solver itself doesn't care about the
  semantics, it just needs rank + dimensions + contiguous memory.

- DEC-003: (revised) Widgets receive SimContext, not Experiment. They
  declare extraction requests and read results from PublishedState.
  No direct access to Simulation or Experiment internals. Resolves
  TBD-005. See doc/simthread.md for extraction pipeline details.

- DEC-004: (revised) Simulation runs on a dedicated std::thread owned
  by SimContext. Triple-buffered PublishedState for sim→UI data flow.
  UI communicates via fire-and-forget command queue. SDL user event
  wakes main loop on new results. Resolves TBD-001. See doc/simthread.md.

- DEC-006: (revised) FFT abstraction is via Solver base class with
  virtual step()/init(). CPU backend (solver_cpu.cpp) uses fftwf plans.
  GPU backend (solver_gpu.cpp) uses VkFFT + OpenCL. For rank > 3,
  GPU decomposes into batched 1D + transpose + batched 3D (VkFFT caps
  at 3D). Solver also owns extraction kernels (slices, marginals,
  norm reduction). Resolves TBD-006.

- DEC-009: Boundary conditions. FFT imposes periodicity (pacman wraparound).
  Default is periodic. Absorbing boundaries are available as a Lua-configurable
  option: a strip of complex potential with smoothly ramping imaginary part
  that drains probability. Width and strength are tweakable per experiment
  in the world definition. Implemented as part of the potential — transparent
  to the solver. Imperfect by nature: too thin reflects, too narrow for a
  spread-out wavefunction lets stuff through. The user sees this and learns.
  Resolves TBD-003.

- DEC-005: SI units throughout. No natural units, no grid units. Positions in
  meters (displayed as nm/μm by humanize()), potentials in eV, time in seconds
  (displayed as fs/ps), mass in kg, momentum in kg·m/s. The Schrödinger
  equation is evaluated with the real physical constants ℏ and m. Float32
  handles the magnitudes fine — the tiny numbers (10⁻³⁴) appear as ratios
  and cancel to give reasonable-scale results. Resolves TBD-004.

- DEC-010: Float32 throughout. psi_t = std::complex<float>. GPU is float32
  natively — no double support on most consumer GPUs. Half the memory vs
  double (critical for 4D: 64⁴ × 8 bytes = 134MB vs 268MB). Precision is
  adequate for visualization and learning. Unitarity drift is visible over
  very long runs — instructive, not a bug. FFTW uses fftwf (float) plans.
  All buffers are std::vector<psi_t>, no fftw_malloc.

## Actionable Items

(all current items done — see Done section)

## Done

- ACT-001: Simulation class — grid, potential, solver, psi vectors, dt, step count. Exposes state via SimContext extraction pipeline.
- ACT-002: Split-step Fourier solver — CPU (fftwf) and GPU (VkFFT+OpenCL) backends. Arbitrary rank. 4D decomposition for VkFFT 3D limit.
- ACT-003: Simulation tick wired via SimContext::poll() on main thread.
- ACT-004: Multiple visualization widgets — helix (3D complex), grid (2D density), info (status), trace (time series).

## Scratch

Agent note: this section is a dumping ground for raw thoughts, ideas, and
half-formed requirements. The agent will periodically review, consolidate
into FR/TBD/ACT entries, and confer with the user when needed.

- snapshot timeline: keep intermittent deep copies of simulation state,
  dense near present, sparse further back (logarithmic spacing). Use as
  restart points for reverse evolution — pick nearest snapshot, reverse-evolve
  from there. Limits accumulated floating point error. Memory cost is
  N_snapshots × grid_size. Not v1 but the architecture should allow it
  (Simulation state must be deep-copyable).
- visualization approaches from README discussion: phase-as-hue magnitude-as-brightness,
  marginals, conditional slices, Schmidt decomposition, Wigner function, mutual
  information heatmaps, linked/coordinated views
- for >2D need projections — this is a visualization concern, not a model concern
- absorbing boundaries: complex potential at edges, tricky to get right
- the 4D case (2 particles in 2D) is the memory wall: 128⁴ × 16 bytes ≈ 4GB
- FFTW_MEASURE or FFTW_PATIENT at startup, not FFTW_ESTIMATE — different from
  the fft project which re-planned constantly
