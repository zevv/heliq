
skip the toys, id like to have these things do the real math, like the grid sim we did before. No abstractions or shortcuts.
7:18 AM
Identified user's demand for rigorous computational solvers

Good. Here's what exists for real numerical Schrödinger equation solving, no shortcuts.

Best match for what you want — Python, real math, good visualization:

qmsolve is a Python package that solves the Schrödinger equation for one and two particles in 1D, 2D, and 3D, with visualizations.
GitHub
It uses exactly the same methods we used — split-step FFT and Crank-Nicolson, with GPU acceleration via CuPy.
GitHub
You define an arbitrary potential as a Python function, it discretizes the Hamiltonian on a grid, and evolves. It also has an interactive eigenstate viewer where you can superpose computed eigenstates and watch the time evolution.
GitHub
This is the closest to "what we built, but more flexible." GitHub: github.com/quantum-visualizations/qmsolve

If you want raw power and have a GPU:

TorchGPE is a GPU-accelerated split-step Fourier solver built on PyTorch.
INSPIRE
It's designed for Bose-Einstein condensate simulations but the core is the same Schrödinger equation. Blazing fast — turns minutes of CPU time into seconds.

If you want full control and don't mind coding everything:

Honestly, what we already built is the template. The split-step Fourier method in NumPy/SciPy is about 50 lines of Python for 1D, maybe 100 for 2D. You define your grid, your potential, your initial state, and loop. FFT, multiply, inverse FFT. You already understand the algorithm — it's the Fourier decompose, rotate each component, recompose procedure we discussed.

For two particles in 2D — which is where it gets interesting for entanglement — you'd need a 4D grid and 4D FFTs. That's where GPU acceleration matters. NumPy on a CPU handles maybe 128⁴ points. CuPy or PyTorch on a GPU handles much more.

For the multi-particle problem specifically:

QuTiP handles arbitrary quantum systems — spin chains, coupled oscillators, open systems with decoherence.
Qutip
It doesn't do spatial wave functions on grids as its primary thing. It works in the abstract Hilbert space directly — you define operators and states as matrices and vectors. Different approach, but more natural for entanglement problems where you don't care about spatial structure.

My honest recommendation: start with qmsolve for spatial wave function stuff. It's exactly the split-step method we used, but properly packaged with visualization and the ability to define arbitrary potentials. You could recreate both our simulations in about ten lines of Python, then start building your own experiments.

ill be coding this myself; GPU is nice but since the output is for human consumption anyway slow is probably not that bad.
7:26 AM
Acknowledged user's self-coding preference and performance tolerance

Right. You're rendering at maybe 30fps on a screen that's a few hundred pixels wide. The bottleneck is your eyes, not your CPU.

For 1D single particle, you need almost nothing. A NumPy array of complex numbers, numpy.fft, and a loop. The whole solver is maybe 20 lines. You already understand every line — it's the algorithm we talked through.

For 2D single particle — the double slit scenario — it's the same thing but with 2D FFTs. NumPy handles that natively. A 512x512 grid runs comfortably in real time on any modern CPU.

For two particles in 1D — the entanglement simulation — it's also a 2D grid, same code structure. That's what we already built.

The wall you'll hit is two particles in 2D. That's a 4D wave function. A 128x128x128x128 grid is 268 million complex numbers. That's where it gets heavy. But even there, you can start with a coarser grid — 64 per dimension — and it'll run, just slower.

The split-step method is numerically stable, preserves unitarity well, and you already understand why it works. The only tricky part is getting the absorbing boundaries right and making sure your potential doesn't create artifacts at the grid edges.

For visualization, the phase-as-color approach we used is the honest one. Magnitude as brightness, phase as hue. That shows you everything without throwing information away.

What language are you thinking?

for >2d some projections are needed, are there 'best practices' for folding stuff down; not just discarding dimensions and losing info (you cant see it because you're looking straight at it) but a smart thing for projecting things that 'keep a bit of all'?
7:29 AM
Catalogued projection techniques preserving multidimensional quantum information

This is a real problem and people have thought about it. A few approaches that actually work.

Marginals — what we already did. Integrate out some dimensions. For the two-particle-in-1D case, we showed the full 2D joint probability and then the two 1D marginals below. That's projecting down by summing. You lose the correlations in the discarded dimensions, but you know exactly what you lost — and you can show multiple marginals side by side to compensate.

Conditional slices. Instead of integrating out a dimension, pin it at a specific value and show what remains. For two particles in 2D, fix particle 2 at position (x₂, y₂) and show the full 2D wave function of particle 1. Then let the user drag particle 2's position around. The way particle 1's distribution shifts as you move particle 2 makes the entanglement directly visible and intuitive. This is arguably the most powerful approach because it shows you exactly what entanglement means — how one particle's state depends on the other.

Mutual information heatmaps. Don't try to show the wave function directly. Instead, compute the correlation between every pair of dimensions. How much does knowing x₁ tell you about y₂? Make a matrix of these correlations. It's lossy, but it gives you a bird's eye view of which dimensions are entangled with which, at a glance.

Schmidt decomposition. This is the smart quantum-specific one. For a two-particle system, you can always decompose the joint wave function into a sum of products:

ψ(x₁, x₂) = λ₁ f₁(x₁)g₁(x₂) + λ₂ f₂(x₁)g₂(x₂) + ...

The λ values are called Schmidt coefficients. If only one is nonzero, the state is factorable. If many are nonzero, it's entangled. The f's and g's are the natural modes of each particle.

This is beautiful for visualization because you can show each term separately. "Here's the first mode of particle 1 paired with the first mode of particle 2, with weight λ₁. Here's the second pair with weight λ₂." Each term is low-dimensional and easy to draw. The number of significant terms tells you how entangled the system is.

Numerically, it's just an SVD. Reshape your 4D wave function into a 2D matrix — rows are particle 1's degrees of freedom, columns are particle 2's — and take the singular value decomposition. The singular values are the Schmidt coefficients.

Phase space — Wigner function. Instead of showing position-space wave function, transform to phase space. For each particle, you get a distribution over both position and momentum simultaneously. The Wigner function can go negative — which is the signature of quantum behavior — and those negative regions are where the interesting stuff is.

For 1D, the Wigner function is 2D — one axis position, one axis momentum. Easy to plot as a heatmap. You can literally see the helix winding translated into a blob shifted off-center in the momentum direction.

Linked views. The real answer for interactive exploration is not one clever projection but several simple ones linked together. Show the 2D joint probability. Show both marginals. Show a conditional slice. Show the Schmidt decomposition. Show the Wigner functions. All simultaneously, all updating in real time. Mouse over a point in one view and the others respond.

This is how experimental physicists actually work with this stuff. No single view shows everything. But a dashboard of coordinated views, each showing a different projection, gives you a surprisingly complete picture.

For your code, I'd start with conditional slices plus marginals. The slice gives you the entanglement story — drag one particle around and watch the other respond. The marginals give you the individual particle picture. Together they cover most of what you need without any fancy math beyond what you're already doing.

