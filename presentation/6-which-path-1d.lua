description("Which-Path Entanglement", [[
Two electrons. A hits a beam splitter from (4), B sits on the far
side. A's transmitted half kicks B; the reflected half doesn't reach
it. Now B is in superposition: kicked and not kicked. They are
entangled — B's state encodes which path A took. Slice through one
particle's axis to see how the other's state depends on it.]])

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
