-- Two particles in 1D with contact interaction
-- Joint configuration space: ψ(x₁, x₂) on a 2D grid
-- Thin barrier along diagonal x₁=x₂ causes partial reflection + tunneling
-- After scatter: two lobes = entangled superposition of outcomes


local L = 5 * um

domain {
    { min = -L, max = L, points = 512 },
    { min = -L, max = L, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.062e-3 * eV
local momentum = math.sqrt(2 * m_electron * energy)

-- electron 1: moving right from the left
particle(electron, {
    position = { -2 * um },
    momentum = { momentum },
    width = 0.3 * um,
})

-- electron 2: moving left from the right
particle(electron, {
    position = { 2 * um },
    momentum = { -momentum },
    width = 0.3 * um,
})

-- contact interaction: thin barrier along diagonal
-- just above kinetic energy, very thin for partial tunneling
interaction {
    type = "contact",
    particles = { 1, 2 },
    strength = energy * 1.5,
    width = 0.02 * um,
}
