# Spin

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

The simulator currently models spinless particles. The `measure()` function
is a bolted-on projection postulate — a random number generator that
collapses the wavefunction. It works, but it's the one part of the
simulator that isn't real physics.

Spin-1/2 eliminates the need for `measure()`. A Stern-Gerlach magnet is
a real potential in the Hamiltonian: inhomogeneous B field couples to the
spin DOF via Pauli matrices, spatially splitting spin-up from spin-down.
The "measurement" is the physical separation — no collapse, no postulate,
just Schrodinger evolution. The detector IS the magnet. The result IS the
position.

This is the Everettian argument made concrete: branching emerges from
unitary evolution when internal DOFs entangle with spatial DOFs.

Target experiments:
- Stern-Gerlach: beam splitting from field gradient (1P/2D)
- Sequential SG: measure z, filter, measure x, measure z again —
  incompatible observables, purely spatial (1P/2D)
- Spin-entangled EPR: singlet state through two SG analyzers at
  different angles, Bell violation visible in joint position
  distribution (2P/1D = 2D grid)

## Functional Requirements

- FR-001: Each grid point stores a 2-component complex spinor
  (spin-1/2). Total state: ψ(x) = [ψ↑(x), ψ↓(x)]. Memory
  doubles relative to scalar case. Stored as two separate planar
  buffers (DEC-001).

- FR-002: Potential step applies a 2x2 Hermitian matrix per grid
  point: V(x) = V₀(x)I + μ_B(B_x σ_x + B_y σ_y + B_z σ_z).
  Scalar potential V₀ acts identically on both components.
  Magnetic coupling mixes them. Simulation owns this step, not
  the solver (DEC-002).

- FR-003: Kinetic step is diagonal in spin space — both components
  get the same exp(-i ħk²/2m dt) phase. FFT runs on each component
  independently. No spin-orbit coupling (non-relativistic).

- FR-004: Magnetic field B(x) defined over the grid. At minimum:
  uniform field and linear gradient (analytic, no callback needed).

- FR-005: Lua API for spin: initial spin state (direction or
  explicit [α, β] amplitudes), magnetic field specification.

- FR-006: Scalar experiments are unaffected. Spinor is opt-in per
  particle. No memory or compute cost when spin is not used (DEC-003).

- FR-007: Normalization is over both components:
  ∫(|ψ↑|² + |ψ↓|²) dx = 1.

- FR-008: Helix widget: primary view is two helices (α and β) on
  the same x-axis, same visual weight, subtle color distinction.
  No labels — behavior under the field IS the label. Lossless
  representation of all 4 real numbers per point.

- FR-009: Helix widget: spin dynamics mode. Plots ⟨Sx⟩(x) on y
  and ⟨Sy⟩(x) on z, with ⟨Sz⟩(x) as a separate flat curve.
  Larmor precession appears as helical winding at Larmor frequency.
  Same visual language as existing Re/Im helix, new physics.

- FR-010: Grid widget: spin-resolved datasource modes alongside
  existing |ψ|², Re, Im, phase. New modes: total ρ (|ψ↑|²+|ψ↓|²),
  spin-z (|ψ↑|²-|ψ↓|²), spin-x (2 Re(ψ↑*ψ↓)),
  spin-y (2 Im(ψ↑*ψ↓)).

- FR-011: Extraction pipeline: return both spin components. Widgets
  compute derived quantities (ρ, Bloch vector, coherence) from the
  raw spinor data. Extraction kernels sum over spin for backwards
  compatibility with scalar datasource modes.

## To Be Decided

- TBD-004: Extraction pipeline details. Do GPU extraction kernels
  (slice, marginal) sum over spin components on-device, or return
  both components and let the CPU sum? On-device is faster for
  marginals; returning both is needed for spin-resolved views.
  Possibly: always return both, sum on CPU for scalar modes.

- TBD-005: 2-particle spin: singlet state (↑↓ - ↓↑)/√2 is a
  4-component spinor at each config-space point (↑↑, ↑↓, ↓↑, ↓↓).
  For 2P/1D on a 512² grid, that's 1M complex floats — fine. But the
  potential matrix is 4x4 (tensor product of two Pauli spaces). Is this
  needed for the Bell test, or can we get away with 2 components per
  particle handled independently?

- TBD-006: Does `measure()` survive alongside spin, or do we fully
  retire it once SG works? measure() is still useful for position
  measurements on entangled pairs (EPR without spin). But philosophically
  it's the thing we're trying to eliminate.

- TBD-007: Helix coherence view: |α·β| vs φ_rel in polar form.
  Radius = spin coherence (zero for pure ↑ or ↓, max for equal
  superposition). Angle = relative phase. Precession = circular
  motion. SG separation = radius collapse. Worth implementing as a
  third helix mode, or overkill?

- TBD-008: Split-step ordering. Half spin rotation, full solver on
  each component, half spin rotation — or integrate the spin rotation
  into the existing potential half-step? The scalar potential and
  magnetic rotation commute ([V₀I, μ·B] = 0), so factoring is exact.
  But the half-step sandwich is cleaner conceptually.

## Decisions

- DEC-001: Planar memory layout — two separate buffers [ψ↑↑...] and
  [ψ↓↓...], not interleaved. FFT dominates runtime and needs contiguous
  access. Potential step (2x2 per point) pays two reads separated by N,
  but it's O(N) simple arithmetic vs O(N log N) FFT. Resolves TBD-001.

- DEC-002: Solver does not know about spin. Simulation owns two planar
  buffers, runs the solver on each independently (same plans, same
  kinetic phase). Simulation handles the 2x2 Pauli potential rotation
  between solver calls. This is exact because [V₀I, μ·B] = 0.
  solver_gpu.cpp, VkFFT plans, transpose kernels, extraction kernels
  are untouched. Resolves TBD-002.

- DEC-003: Scalar path stays default. Spinor is opt-in per particle.
  Existing experiments pay no memory or compute cost. Resolves TBD-003.

## Actionable Items

- ACT-001: Add spin flag and initial spin state to Setup/Particle.
  Lua API: spin_state = {alpha, beta} or spin_direction = {theta, phi}.
  Default: no spin (scalar path).

- ACT-002: Allocate second psi buffer in Simulation when spin is
  enabled. Initialize both components from initial spin state ×
  spatial wavepacket.

- ACT-003: Implement spin rotation step in Simulation. Per grid point:
  compute θ = μ_B|B|dt/2ħ, n̂ = B/|B|, apply rotation matrix
  cos(θ)I - i sin(θ)(n̂·σ) to (ψ↑, ψ↓). Run as half-step before
  and after solver.

- ACT-004: Add magnetic field to Setup. Lua API: magnetic_field{}
  with type = "uniform" or "gradient". Store as B(x) sampled on grid
  or as analytic parameters.

- ACT-005: Modify solver step loop: half spin rotation → solver on
  ψ↑ → solver on ψ↓ → half spin rotation. Scalar path unchanged
  (no spin rotation, single solver call).

- ACT-006: Update extraction pipeline to return both spin components
  when spin is active. Backwards-compatible: scalar experiments
  return single component as before.

- ACT-007: Helix widget: two-helix mode for spinor data. Two curves,
  same axis, subtle color distinction, no labels.

- ACT-008: Helix widget: Sx/Sy spin dynamics mode. Compute Bloch
  vector from extraction data, plot ⟨Sx⟩ on y, ⟨Sy⟩ on z. Sz as
  flat curve below.

- ACT-009: Grid widget: add spin datasource modes (ρ, Sz, Sx, Sy).

- ACT-010: First experiment: 1P/2D Stern-Gerlach. Spin-x initial
  state, B_z gradient in y. One blob in, two blobs out.

- ACT-011: Second experiment: sequential SG. SG-z → filter one beam
  → SG-x → SG-z. Demonstrates incompatible observables.

## Done

## Scratch

Split-step with spinor:
1. Half spin rotation: apply exp(-i μ·B dt/2ħ) per point.
   exp(-i θ n̂·σ) = cos(θ)I - i sin(θ)(n̂·σ)
   where θ = μ_B|B|dt/2ħ, n̂ = B/|B|. Single trig eval per point.

2. Solver on ψ↑: potential phase × FFT × kinetic phase × IFFT.

3. Solver on ψ↓: same, identical operation.

4. Half spin rotation again.

Cost vs scalar: 2x FFT, 2x kinetic phase (trivial), spin rotation
is 2x2 matrix-vector product per point (4 muls + 2 adds). Total:
roughly 2x compute, 2x memory. Spin rotation is negligible vs FFT.

Stern-Gerlach field profile:
- B_z(y) = B₀ + G·y (linear gradient in y)
- Gradient G creates force F_z = ±μ_B · G (opposite for ↑ and ↓)
- Classical deflection after time t: Δy = ±(μ_B G / 2m) t²
- Field must be localized (particle enters, passes through, exits)
  or uniform (simpler, less physical)

The 2x2 potential matrix for pure SG (B_z only):
```
V(x,y) = [ -μ_B B_z(y)     0          ]
          [  0               μ_B B_z(y) ]
```
Diagonal — no spin flip. Sequential SG needs off-diagonal terms
(rotated field direction) which requires the full rotation machinery.

Bell test geometry (2P/1D):
- Two particles on a 1D line, initial singlet state
- Particle 1 enters SG magnet at angle α, particle 2 at angle β
- Joint state is 4-component: |↑₁↑₂⟩, |↑₁↓₂⟩, |↓₁↑₂⟩, |↓₁↓₂⟩
- After SG splitting: 4 spatial lobes in config space
- Probability in each lobe depends on (α - β) — Bell correlations
- Grid: 512² × 4 components = 1M complex floats, trivial
- Requires TBD-005 resolved first

Visualization quantities — derived from (α(x), β(x)):

Probability density: ρ(x) = |α|² + |β|²
Probability current: j(x) = Im(α* ∂α/∂x + β* ∂β/∂x)
Spin vector (Bloch field):
  ⟨Sx⟩(x) = 2 Re(α* β)
  ⟨Sy⟩(x) = 2 Im(α* β)
  ⟨Sz⟩(x) = |α|² − |β|²
Spin current: Q(x) ∝ |α|² ∂(arg α)/∂x − |β|² ∂(arg β)/∂x

Natural helix pairings:
- ⟨Sx⟩/⟨Sy⟩ helix: Larmor precession = winding. Best for field
  interactions. Radius = transverse spin coherence.
- |α·β| vs φ_rel polar: coherence view. Radius collapses in SG,
  uniform circular motion under precession.
- |α|²/|β|² not a helix: curve peels from diagonal in SG.

Losslessness: (ρ, j, S, Q) = 4 real numbers per x = complete.
Two helices in 3D is also lossless: radius + winding of each =
|α|, arg α, |β|, arg β. All views are coordinate transforms of
the same 4 numbers.
