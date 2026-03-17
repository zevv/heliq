# heliq — Agent Onboarding

Real-time interactive quantum mechanics simulator. Split-step Fourier solver
on GPU, ImGui visualization, Lua experiment scripts. Non-relativistic
Schrodinger equation, no shortcuts, real SI units, real physical constants.

## Who is the user

A technically proficient developer with DSP experience and linear algebra
basics, learning QM by building tools. Has read Carroll (Something Deeply
Hidden), Maudlin (Philosophy of Physics), Albert (Quantum Mechanics and
Experience) — knows the *ideas* of superposition, entanglement, measurement,
but wants to build physical intuition through direct manipulation. Not a
physicist. Expects to set up experiments with real units, tweak parameters,
and see what happens.

## What this does

Solves the time-dependent Schrodinger equation on an N-dimensional grid.
N = particles x spatial_dims. One particle in 2D = 2D grid. Two particles
in 1D = 2D configuration space grid. Two particles in 2D = 4D grid (64^4
= 16.8M points, runs on GPU).

The user defines experiments in Lua: particles, potentials, interactions,
domain. The simulator evolves the wavefunction and multiple synchronized
visualization widgets display different projections of the same state.

## Architecture overview

```
experiments/*.lua  →  lua/prelude.lua  →  model/loader.cpp  →  Setup
                                                                  ↓
                                                            Simulation
                                                           (Grid, ConfigSpace,
                                                            Solver, psi, potential)
                                                                  ↓
                                                         Experiment (owns N sims)
                                                                  ↓
                                                    App → Panel tree → Widgets
                                                              (helix, grid, info)
```

### model/ — Physics engine

- `grid.hpp` — N-dimensional regular grid. Axis metadata (points, min, max,
  label). Strides are **private** — all data access goes through `axis_view()`
  and `slice_view()` to prevent raw indexing bugs. These return zero-copy
  strided views with iterators.

- `configspace.hpp` — Maps particles to config-space axes. `axis(particle, dim)`
  gives the config-space axis index. `distance_sq(pa, pb, pos)` computes
  Euclidean distance between particles in physical space. `label_axes(grid)`
  generates axis labels (P1.x, P2.y, etc). This is the single source of truth
  for the particle-to-axis mapping.

- `setup.hpp` — Pure data output from Lua ingestion. Particles, potentials,
  interactions, domain, title, description. Immutable after creation.

- `simulation.hpp/cpp` — Owns the wavefunction, potential, phase arrays (as
  `std::vector<psi_t>`), and the solver. `psi_t = std::complex<float>` — the
  entire pipeline is float32. GPU operates in float32 natively; there is no
  double↔float conversion anywhere.

  Key methods: `step_compute()` (queue GPU work), `flush()` (wait for GPU),
  `sync()` (read psi back to CPU — expensive, lazy via `m_psi_dirty`),
  `commit_psi()` (push modified CPU psi to GPU + swap display buffers),
  `normalize_psi()`, `measure(axis)`, `decohere(axis)`.

  **Tricky**: `psi_front()` triggers a full GPU readback if dirty. Widgets
  that only need slices/marginals should use `read_slice_2d()` or
  `read_marginal_2d()` instead — these run GPU kernels that return only
  the small result, not the full wavefunction.

- `solver.hpp` — Abstract split-step Fourier solver. CPU (FFTW) and GPU
  (VkFFT + OpenCL) backends behind one interface. The solver owns all
  hot-path data on its side (device buffers, FFT plans, phase arrays).

- `solver_gpu.cpp` — **The trickiest file in the project.** VkFFT caps at
  3D. For rank > 3 (the 4D case), the solver decomposes the FFT:
  1. Batched 1D FFT on the innermost axis (contiguous, trivial)
  2. GPU transpose: [N0][N1][N2][N3] → [N3][N0][N1][N2]
  3. Batched 3D FFT on axes 0,1,2 (now contiguous blocks)
  4. GPU transpose back
  
  The kinetic phase array is pre-transposed at upload time to avoid extra
  transposes per step. This decomposition was verified against FFTW at 8^4
  and 64^4 (see test/test_vkfft4d_transpose.cpp).

  Also contains GPU-side extraction kernels: `extract_slice_2d` (2D kernel),
  `marginal_2d` (per-output-pixel reduction), `reduce_norm_sq` (parallel
  reduction for total probability).

- `solver_cpu.cpp` — FFTW float (fftwf) backend. Supports arbitrary rank
  natively. Saves/loads FFTW wisdom to `~/.cache/heliq/fftwf_wisdom`.

- `experiment.hpp/cpp` — Owns simulations, manages advance loop with adaptive
  batch sizing. `load()` preserves user-adjusted timescale and dt across
  reloads (R key).

### ui/ — Application framework

- `app.cpp` — Main loop, event handling, panel management. Per-experiment
  config saved to `~/.config/heliq/<experiment-name>`.

- `view.hpp` — Shared state across widgets: cursor positions (int per axis),
  Camera3D, amplitude. Widgets with `lock=true` sync their camera with the
  shared view.

- `camera3d.hpp` — Reusable 3D camera: orbit (MMB), pan (Shift+MMB), zoom
  (scroll), numpad views (1/3/7/5), ortho toggle. Owns its own save/load
  with prefix support.

- `colors.hpp` — Central color definitions. No magic RGB values in widget code.

- `panel.cpp` — Recursive split panel tree. Panels can be split H/V, widgets
  assigned via F-keys (F1=info, F2=grid, F3=helix).

- `math3d.hpp` — Minimal mat4/vec3 with look_at, ortho, perspective, inverse.

- `misc.hpp` — Shared utilities: `humanize()` (SI prefix formatting),
  `ToggleButton`, `AxisCombo` (dropdown for axis selection with labels).

### widget/ — Visualization

- `widget-helix.cpp` — The flagship widget. 3D helix view of 1D slices:
  x-axis = position, y = Re(psi), z = Im(psi). Multiple layers: helix line,
  surface fill, envelope, potential overlay. Each layer has its own color
  palette (default/gray/rainbow/flame/spatial). Supports Slice/Marginal/Momentum
  modes. The spatial color mode maps config-space position to hue, with
  saturation indicating entanglement (low saturation = mixed origins in the
  marginal = entangled).

- `widget-grid.cpp` — 2D grid view with SDL texture overlays. For rank > 2,
  allows axis pair selection (which 2 of N axes to display). Slice mode shows
  data at cursor; Marginal mode sums over hidden axes (GPU-accelerated).
  Uses `read_slice_2d()` / `read_marginal_2d()` — never reads full psi for
  rendering.

- `widget-info.cpp` — Transport controls, dt/speed sliders (log scale, ranges
  centered on auto-computed defaults), phase stability indicators, Nyquist
  aliasing check, experiment title/description, setup summary. 'A' key resets
  speed/dt to auto-computed defaults.

- `datasource.hpp` — Shared enums and functions for data sources (|psi|^2,
  Re, Im, phase, potential) and color palettes (flame, gray, rainbow, zebra,
  spatial). Used by both grid and helix widgets.

### lua/ — Experiment language

- `prelude.lua` — Loaded before every experiment script. Defines the API:
  `description()`, `domain()`, `def_particle()`, `particle()`, `barrier()`,
  `well()`, `harmonic()`, `interaction()`, `absorbing_boundary()`, `simulate()`.
  Auto-computes dt (10% of stability limit) and timescale (20% domain crossing
  in 5 wall seconds). Checks Nyquist aliasing and wavepacket resolution.

### experiments/ — Numbered 010-200

Ordered for learning: 1P/1D basics → 1P/2D spatial → 2P/1D config space →
2P/2D full quantum eraser. Each script has a `description()` call that
shows title + explanation in the info widget.

## Key design decisions

- **float32 throughout** (`psi_t = std::complex<float>`). GPU is float32
  natively. No conversion overhead. Half the memory. Precision is adequate
  for visualization.

- **Grid stride is private.** All N-dimensional data access goes through
  `axis_view()` / `slice_view()` / `grid.each()`. This prevents the entire
  class of bugs where code assumes wrong axis ordering.

- **ConfigSpace** maps particles to axes formally. Interactions use
  `cs.distance_sq()`, not raw index arithmetic. Axis labels are generated
  from the mapping, not hardcoded.

- **Lazy GPU readback.** `sync()` (full 16.8M readback) only happens when
  `psi_front()` is called (measure, decohere, dump). Widgets use GPU-side
  extraction kernels for slices and marginals.

- **Gaussian contact interaction.** Changed from hard box to smooth
  `V = strength * exp(-r^2/w^2)`. Prevents wall-like artifacts, allows
  particles to pass through with phase shifts instead of bouncing.

## What is NOT done yet

- Sim thread (simulation still blocks UI during step)
- Time-dependent potentials
- Spin (TBD-008 in model.md)
- Magnetic fields (TBD-009)
- Schmidt decomposition / entanglement entropy display
- Wigner function view
- The 2D quantum eraser interaction needs tuning — partial which-path
  info but not complete interference destruction yet

