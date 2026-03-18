# Simulation Thread

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

Simulation currently blocks the UI thread during stepping. On large grids
(64^4 = 16.8M points) a single step batch can take tens of milliseconds,
causing UI stutter. The simulation must run on its own thread.

The core tension: the simulation owns GPU state (OpenCL command queue,
device buffers) which cannot be safely accessed from multiple threads.
Widgets currently call extraction methods (read_slice_1d, read_marginal_2d,
total_probability) that submit GPU work on the same command queue the
solver uses for stepping. This must be untangled.

## Functional Requirements

- FR-001: Simulation runs on a dedicated thread, UI never blocks on sim
  stepping.
- FR-002: UI sends commands to the sim thread via a command queue.
  Commands are fire-and-forget from UI perspective. Sum type
  (`std::variant`) for compile-time exhaustiveness.
- FR-003: Sim thread is the sole owner of all GPU state. No GPU calls
  from the UI thread.
- FR-004: UI declares extraction requests (slices, marginals) each frame.
  Sim thread fulfills them between step batches. Results are broadcast —
  all widgets scan the result set and pick what they need.
- FR-005: Results are published via a triple-buffered slot with overwrite
  semantics. Model never waits for UI. UI never waits for model.
- FR-006: Extraction requests are deduplicated by the UI before
  publishing. Two widgets wanting the same data = one extraction.
- FR-007: Reload (R key) is a command to the sim thread. Sim thread
  drains, tears down, rebuilds internally. Long-lived thread, no
  create/destroy per reload.
- FR-008: Mutating operations (measure, decohere, set_dt, set_absorb)
  are commands. Sim thread applies them between step batches.
- FR-009: Published state includes: extraction results, sim_time,
  step_count, total_probability, phase indicators (V, K), running state,
  grid metadata (rank, axes, labels, configspace).
- FR-010: First frame renders empty — no data yet. Requests are sent,
  results arrive next frame. One frame of latency, invisible at 60fps.
- FR-011: Sim thread sleeps when paused. Wakes on command (resume,
  single-step, load, measure).

## To Be Decided

- TBD-001: (resolved → DEC-004)
- TBD-002: (resolved → DEC-007)
- TBD-003: (resolved → DEC-008)
- TBD-004: (resolved → DEC-009)
- TBD-005: (resolved → DEC-010)
- TBD-006: (resolved → DEC-011)
- TBD-007: (resolved → DEC-012)

## Decisions

- DEC-001: UI posts wall_dt to sim thread via CmdAdvance each frame.
  Sim thread uses it with timescale to compute step count. Sim thread
  has no clock of its own. More testable — can feed synthetic wall_dt.
- DEC-002: CmdLoad carries Lua source string, not filename. UI owns
  filesystem I/O. Sim thread never touches files. This enables a future
  in-UI Lua editor without changing the sim thread interface.
- DEC-003: Sim thread is the sole owner of all GPU state and all model
  state. Share-nothing. UI communicates exclusively through command
  queue (UI→sim) and published state (sim→UI).
- DEC-004: Unified extraction interface. slice() and marginal() are
  generic over rank — axes list determines dimensionality. 1D, 2D, or
  higher, same request struct. Model dispatches to the right GPU kernel
  based on axes count. Resolves TBD-001.
- DEC-005: Extraction results are always complex (psi_t). Marginals
  store real part only, imaginary zeroed. One data type, one buffer,
  one code path. No variant in result.
- DEC-006: Potential is extracted alongside psi in every extraction
  result. Same axes, same cursor. ExtractionResult carries both
  data (psi) and potential vectors. Handles future time-dependent
  potentials without interface change.
- DEC-007: Trace widget accumulates whenever new data arrives (check
  step_count changed in published state). No per-step guarantee.
  Captures at frame rate at best. step_interval slider dropped —
  trace captures every new published state. Resolves TBD-002.
- DEC-008: Single-step is CmdSingleStep{}. One frame latency.
  Acceptable for interactive use at 60fps. Resolves TBD-003.
- DEC-009: Generation counter in published state. Incremented on
  load/reload. Widgets compare against their cached generation —
  changed → reconfigure (reset trace buffer, clamp axis indices,
  etc). Cheap, no string compare. Resolves TBD-004.
- DEC-010: Log infrastructure shared between UI and model. 5 levels
  (err/wrn/inf/dbg/dmp), tag from __FILE__, stderr with color.
  Both threads use same log macros. No printfs/fprintfs for regular
  logging. fprintf to stderr is thread-safe on POSIX.
  Errors in published state: string field for fatal/blocking errors
  (load failure, GPU init failure). Log for everything else.
  Resolves TBD-005.
- DEC-011: FFTW is safe for this design. Plan creation (not thread-safe)
  only happens on sim thread during solver init. fftwf_execute (thread-
  safe) can be called concurrently from UI (helix momentum FFT) and sim
  thread with separate plans. No issue. Resolves TBD-006.
- DEC-012: total_probability is part of published state, computed by
  the sim thread every batch (not every step — one GPU reduction per
  batch is enough). Resolves TBD-007.

## Actionable Items

- ACT-001: Define shared types. New header `model/simtypes.hpp`:
  SimCommand variant, ExtractionRequest, ExtractionResult,
  ExtractionSet, PublishedState, GridMeta. No implementation,
  just the data definitions. Both threads include this.

- ACT-002: Triple-buffer template. `TripleBuffer<T>` with
  `write()` (producer publishes), `read()` (consumer gets latest).
  Lock-free, atomic index. Put in `model/triplebuf.hpp`.

- ACT-003: Command queue. SPSC lock-free ring or mutex+deque.
  `SimCommandQueue`: `push(SimCommand)` from UI, `drain(callback)`
  from sim thread. Condvar for waking sim thread when idle.
  Put in `model/simqueue.hpp`.

- ACT-004: SimThread class. `model/simthread.hpp/cpp`. Owns the
  thread, command queue, request double-buffer, result triple-buffer.
  Public interface: `start()`, `stop()`, `push(SimCommand)`,
  `publish_requests(ExtractionSet)`, `read_state() → PublishedState*`.
  Internal loop: drain cmds → step batch → extract → publish.

- ACT-005: (revised) Wire SimThread into App alongside Experiment.
  App owns both. SimThread starts, receives CmdLoad, CmdAdvance,
  publishes state. Widgets still read from Experiment directly.
  Proves plumbing works. Compiles and runs at every step.

- ACT-006: (revised) Add SimContext to widget interface as second
  parameter (Experiment& stays). Widgets can optionally read from
  SimContext. Migrate one widget at a time to read from published
  state, starting with the simplest (info, then grid, then trace,
  then helix).

- ACT-007: (revised) Once all widgets read from SimContext only,
  remove Experiment& from the widget interface. App stops populating
  Experiment, relies on SimThread entirely.

- ACT-008: (revised) Delete direct Simulation access from widgets.
  Remove Simulation::read_slice_*, psi_front(), etc from public API.

- ACT-009: Replace all fprintf/printf logging with log macros
  across codebase.

## Done

- ACT-001: simtypes.hpp — SimCommand variant, ExtractionRequest/Set/Result, PublishedState, GridMeta
- ACT-002: triplebuf.hpp — lock-free TripleBuffer<T> template
- ACT-003: simqueue.hpp — SimCommandQueue with mutex+condvar

## Scratch

- Command sum type:
  CmdAdvance{double wall_dt}, CmdSetDt{double}, CmdSetTimescale{double},
  CmdSetRunning{bool}, CmdMeasure{int axis}, CmdDecohere{int axis},
  CmdSetAbsorb{bool, float w, float s}, CmdLoad{string lua_source},
  CmdSingleStep{}

- Extraction request:
  struct ExtractionRequest {
      int axes[MAX_RANK];    // free axes, -1 terminated
      int cursor[MAX_RANK];  // position on fixed axes (ignored for marginal)
      bool marginal;         // false=slice, true=marginal
  };
  Deduplication: equal if same axes, same marginal, same cursor (slices).
  UI builds set each frame, publishes via double-buffered slot.

- Extraction result:
  struct ExtractionResult {
      int axes[MAX_RANK];    // what was extracted
      bool marginal;
      int shape[MAX_RANK];   // points per axis, -1 terminated
      std::vector<psi_t> data;  // always complex, marginal: im=0
  };
  Widgets scan result bag, match by axes+marginal, render.

- Published state (triple-buffered, overwrite semantics):
  struct PublishedState {
      ExtractionResult results[8];
      int n_results;
      double sim_time;
      size_t step_count;
      double total_probability;
      double phase_v, phase_k;
      double max_amplitude;
      int marginal_peaks[MAX_RANK];  // argmax per axis (for auto-track)
      GridMeta grid;     // rank, axes, labels, configspace
      std::string error; // empty = ok
  };

- Request path: double-buffered slot, UI writes back, atomic swap, sim
  reads front.
- Result path: triple-buffered slot (overwrite dequeue), sim writes,
  UI reads latest.
- Sim thread loop: while(alive) { drain commands; apply; if have
  wall_dt budget: step batch; read request set; extract; publish;
  if idle: condvar wait for next command }
- psi never crosses thread boundary. Widgets never see raw psi_t*.
  Everything goes through extraction results.
- Camera, cursor, panel state — purely UI-side, no change needed.
- CmdLoad carries source string. UI reads file / editor buffer.
- Pacing: timescale already controls sim-time per wall-second.
  default_timescale is auto-computed by prelude.lua ("20% domain
  crossing in 5 wall seconds"). No new mechanism needed.
- Helix spatial hue: widget requests slice_2d(0,1), computes hue/sat
  locally. No special extraction type. Rank>2 already skips this.
- Auto-track: model computes marginal peaks, publishes in state.
  UI moves cursor if auto_track enabled. No model→UI callback.
