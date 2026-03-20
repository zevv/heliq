description("Which-Path Entanglement", [[
Particle A moves right through a 50% beam splitter — it partially
reflects, partially transmits. Particle B sits past the barrier,
coupled to A via contact interaction.

Only A's transmitted component reaches B and kicks it. After the
interaction, B is in a superposition: kicked (A transmitted) and
not kicked (A reflected). The two particles are now entangled —
B's state encodes which path A took.

Watch B's marginal: it splits into two peaks, one stationary (A
reflected, no kick) and one moving (A transmitted, B recoils).
This is the "which-path marker" in quantum eraser experiments.

Measure A (M key): B collapses to kicked or not kicked.
Measure B (N key): A collapses to transmitted or reflected.
The outcomes are always correlated.]])

domain {
    { min = -150 * nm, max = 150 * nm, points = 512 },
    { min = -150 * nm, max = 150 * nm, points = 512 },
}

electron = def_particle {
    mass   = m_electron,
    charge = -e_charge,
}

local energy   = 0.1 * eV
local momentum = math.sqrt(2 * m_electron * energy)

-- particle A: signal, moving right toward the beam splitter
particle(electron, {
    position = { -40 * nm },
    momentum = { momentum },
    width    = 5 * nm,
})

-- particle B: idler, stationary, past the barrier
particle(electron, {
    position = { 30 * nm },
    momentum = { 0 },
    width    = 5 * nm,
})

-- beam splitter: ~50% transmission
barrier {
    from   = { -21.5 * nm, -200 * nm },
    to     = { -18.5 * nm,   200 * nm },
    height = energy * 0.85,
}

-- which-path marker: contact interaction
-- fires when A's transmitted component reaches B's position
interaction {
    type      = "contact",
    particles = { 1, 2 },
    strength  = energy * 2,
    width     = 4 * nm,
}
