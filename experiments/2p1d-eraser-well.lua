-- Quantum eraser: which-path information destroys interference.
--
-- Particle A (axis 1) moves rightward through a 50% barrier (beam splitter).
-- Without B: A partially reflects, partially transmits, the two components
-- interfere wherever they overlap.
--
-- Particle B (axis 2) sits near the barrier, contact-coupled to A.
-- When A passes through the barrier, x_A ≈ x_B fires the interaction,
-- kicking B and entangling the two particles. This tags which-path info
-- onto B, destroying A's interference.
--
-- Protocol:
--   1. Run as-is: A's marginal shows no interference (which-path in B)
--   2. Reload (R), decohere B (Shift+N) after interaction: interference returns
--   3. Reload (R), measure B (N) after interaction: A collapses to one path

-- You're right — reflected A is moving rightward, transmitted A is stopped.
-- They don't collide head-on, reflected A just passes through the stopped one
-- and keeps going. They overlap briefly, then separate again. Not a clean
-- recombination. For real interference you need the two components moving in
-- the same direction, or both stationary, overlapping in the same region. Here
-- one is moving and one is stopped — they'll overlap momentarily and you might
-- see a brief flash of interference fringes in the marginal, but it won't
-- persist. This is fundamentally the problem with the 1D setup. In a
-- Mach-Zehnder both paths arrive at the same point from the same direction
-- with the same momentum. Here the two A-components have different momenta
-- (one moving, one stopped), so their overlap is fleeting. The experiment
-- as-is still demonstrates entanglement (measurement collapses B, two blobs,
-- etc.) but the eraser — showing interference reappear after decohereing B —
-- needs the two paths to coexist in the same region long enough to see
-- fringes. Honestly, this might work better with a harmonic well on A's axis.
-- A oscillates, the two components naturally refocus at the turning point with
-- zero momentum. Both stopped, same place, perfect overlap. The contact
-- interaction fires at some point during the oscillation.

local L = 5 * um

domain {
    { min = -1.2 * L, max = 0.8 * L, points = 512 },
    { min = -0.2 * L, max = 1.8 * L, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.062e-3 * eV
local momentum = math.sqrt(2 * m_electron * energy)

-- particle A: signal, moving right toward the barrier
particle(electron, {
    position = { -1.5 * um },
    momentum = { momentum },
    width = 0.25 * um,
})

-- particle B: idler, stationary, past the barrier
particle(electron, {
    position = { 1.5 * um },
    momentum = { 0 },
    width = 0.25 * um,
})

-- beam splitter at x_A=0: must span all of x_B in config space
barrier {
    from = { -0.5 * um + -0.02 * um, -L },
    to   = { -0.5 * um +  0.02 * um,  L },
    height = energy * 1.1,
}

-- which-path marker: contact interaction on |x_A - x_B| < width
-- B is at 3um, so this fires when A reaches 2-4um — well past the barrier
-- only A's transmitted component enters this zone
interaction {
    type = "contact",
    particles = { 1, 2 },
    strength = energy * 2,
    width = 0.2 * um,
}

-- no absorbing boundary: domain walls act as mirrors for A
-- absorbing_boundary {
--     width = 0.07,
-- }
