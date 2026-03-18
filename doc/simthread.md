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
  step_count, total_probability, phase indicators (V, K), dt, timescale,
  running state, grid metadata (rank, axes, labels, configspace),
  setup metadata (title, description, n_particles), marginal peaks.
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
- DEC-005: (revised) Extraction results always carry all derived
  quantities for the requested axes/cursor. No per-field flags.
  For every request the model always computes:
  - psi: complex slice, or |ψ|² marginal (real, im=0)
  - pot: complex potential slice, or |ψ|²-weighted potential marginal
  - coherent: ∫ψ over hidden axes (marginal only; empty for slices)
  All stored as psi_t (complex). Cost is sub-millisecond for all three;
  no reason to make widgets opt-in per field.
- DEC-006: (revised) Superceded by DEC-005. Potential and coherent
  marginal always bundled with psi in every extraction result.
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

### Phase 1: Async API facade (synchronous, single-threaded)

Define an async API on top of the current model. All widget/app access
to the model goes through this API. Under the hood, `poll()` executes
synchronously on the main thread — drain commands, step, extract,
publish. System stays working at every step during migration.

- ACT-001: Define shared types in `model/simtypes.hpp`:
  SimCommand variant, ExtractionRequest, ExtractionResult,
  ExtractionSet, PublishedState, GridMeta.

- ACT-002: Triple-buffer template `model/triplebuf.hpp`.

- ACT-003: Command queue `model/simqueue.hpp`.

- ACT-004: SimContext class — the async API facade.
  `model/simcontext.hpp/cpp`. Owns Experiment, command queue,
  extraction request set, published state. Public interface:
  `push(SimCommand)`, `publish_requests(ExtractionSet)`,
  `poll()`, `state() → const PublishedState&`.
  `poll()` is synchronous: drain commands → step → extract → publish.
  This is the only interface widgets and app use.

- ACT-005: Wire SimContext into App. App owns SimContext.
  Load goes through `push(CmdLoad{source})`. Advance goes through
  `push(CmdAdvance{wall_dt})`. App calls `poll()` each frame.
  Widgets still read from Experiment directly (transitional).

- ACT-006: Migrate widgets to read from `state()` instead of
  poking Simulation. One widget at a time: info → grid → trace → helix.
  Each widget declares extraction requests via `publish_requests()`.
  Experiment& stays in widget interface during migration.

- ACT-007: Remove direct Simulation/Experiment access from all
  widgets. Widget interface drops Experiment&, receives only
  `const PublishedState&`. SimContext is the sole model interface.

### Phase 2: Thread detach

Replace the synchronous `poll()` with a real thread. The API surface
does not change — widgets don't know the difference.

- ACT-008: SimThread replaces the guts of SimContext. `poll()` becomes
  "swap in latest published state from triple buffer". Commands go to
  the queue. Model free-runs on its own thread, lockstepped to wall
  time via timescale, or max speed.

- ACT-009: Replace all fprintf/printf logging with log macros
  across codebase.

## Done

(none — proto implementations exist for ACT-001/002/003 but are
not validated or integrated)

## Scratch

### Two-phase migration strategy

Phase 1 introduces SimContext as a synchronous facade. All model access
goes through it. `poll()` runs inline on the main thread. Widgets
migrate one at a time from poking Simulation to reading PublishedState.
System always compiles and runs.

Phase 2 replaces `poll()` with a real thread. The API surface is
identical. Widgets don't know the difference.

### Extraction pipeline

For every ExtractionRequest (axes + cursor + marginal flag), the model
computes all derived quantities in one pass. No per-field opt-in.

  ExtractionResult {
      axes[], marginal, shape[]
      psi       // slice: complex; marginal: |ψ|² (im=0)
      pot       // slice: complex V; marginal: |ψ|²-weighted V
      coherent  // marginal only: ∫ψ over hidden axes (complex)
  }

Cost for 512² rank-2: sub-millisecond for all three. Negligible vs
the FFT step cost.

Deduplication: UI consolidates widget requests into ExtractionSet.
Same (axes, cursor, marginal) = one request. Multiple widgets share
the result.

### Always-published state (no request needed)

- Scalars: sim_time, step_count, total_probability, phase_v, phase_k,
  dt, timescale, running
- Grid metadata: rank, axes, configspace, labels
- Setup metadata: title, description, n_particles
- Marginal peaks: argmax of |ψ|² per axis (for auto-track)
- Generation counter (incremented on load/reload)
- Error string (non-empty = fatal)

### Invariants

- psi never crosses the API boundary. Widgets never see raw psi_t*.
- Camera, cursor, panel state — purely UI-side, no change needed.
- CmdLoad carries source string. UI reads file / editor buffer.
- Pacing: timescale controls sim-time per wall-second. Auto-computed
  by prelude.lua. No new mechanism needed.
- Auto-track: model computes marginal peaks, publishes in state.
  UI moves cursor if auto_track enabled. No model→UI callback.
