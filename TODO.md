# TODO

## Experiments

### Quantum Eraser — done right (2P/2D, 4D config space)

200-2D-2P-eraser.lua exists but needs tuning. The goal:

1. Particle A through double slit → interference on screen
2. Detector B near one slit, interaction records which-path
3. A's marginal: fringes destroyed (which-path info exists in joint state)
4. Measure B in momentum basis (not position) → A's conditional
   distribution shows fringes again. The info was there, you chose
   not to look at it, and interference came back.

Missing piece: need to show *conditional* marginal (A's distribution
given B's measurement outcome). Currently measure(axis) collapses
the joint state — the remaining marginal IS the conditional. Run
multiple times, see fringes emerge in the post-measurement subset.

Tuning needed:
- Interaction strength: too weak = no which-path, too strong = deflection
- Detector mass: heavy enough to not recoil much, light enough to
  distinguish momentum states
- Slit geometry vs wavelength vs grid resolution at 64^4

### EPR Correlations (2P/1D)

Prepare entangled pair (use 170's collision to generate Bell-like state).
After separation, measure particle 1 with different "basis" choices
(apply momentum kick before measuring = rotated basis). Show that
particle 2's post-measurement state depends on what you did to
particle 1 — not what you measured, but how you chose to measure.

This is the actual EPR argument running live. The correlations are
visible in the marginals after collapse.

### GHZ State (3P/1D = 3D grid)

Three particles, pairwise contact interactions, prepared so measuring
any one determines the other two. 64^3 = 262K points — trivial on GPU.

The point: GHZ correlations are *stronger* than Bell. A single
measurement run disproves local hidden variables (no statistics
needed, unlike Bell which needs many runs). The all-or-nothing
correlation is visible in the 3-particle joint probability.

### Bound State Formation (2P/1D)

Attractive interaction (negative strength), two slow particles.
They can form a transient bound pair: joint wavefunction localizes
along the diagonal (x1 ≈ x2) instead of separating. The marginals
show both particles tracking each other.

Contrast with repulsive: same setup, positive strength, they
bounce. The sign of the interaction determines whether you get
a molecule or a scattering event.

## Visualization

### Conditional Marginal Display

Needed for quantum eraser. After measure(axis), the remaining
marginal on other axes is the conditional distribution. But you
want to show this *without* collapsing — project onto a specific
slice of the measured particle's axis and show the resulting
marginal. This is what ExtractionRequest with fixed cursor +
marginal over remaining axes gives you, but the UI needs a way
to present "if B is here, A looks like this" interactively.

### Entanglement Diagnostic

ExtractionResult.coherent (∫ψ over hidden axes) is computed every
frame, never displayed. |coherent|² / marginal is a direct measure:
= 1 for product states, < 1 for entangled states. The spatial color
mode in helix maps this to saturation, but there's no numeric
readout or per-axis breakdown.

Schmidt decomposition would give the real entanglement entropy.
Compute SVD of the bipartite wavefunction reshaped as a matrix.
Single non-zero singular value = product state. Multiple = entangled.
The entropy of the squared singular values is the entanglement entropy.

## Engine

### Time-Dependent Potentials

Lua callback per timestep to update V(t). Needed for:
- Adiabatic vs sudden potential changes
- Driven systems (oscillating barriers)
- Measurement as a process (gradual decoherence)

Requires keeping Lua state alive after setup (lua-api.md TBD-005).

### Spin

TBD-008 in model.md. Each grid point becomes a (2s+1)-component
spinor. Potential becomes a matrix. Pauli matrices for spin-1/2.
Needed for Stern-Gerlach, spin-orbit coupling, actual magnetic
fields. Deep work — changes the solver fundamentally.
