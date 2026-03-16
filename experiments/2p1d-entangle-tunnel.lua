-- Two particles with contact interaction on the diagonal x₁=x₂.
-- Joint configuration space: ψ(x₁, x₂) on a 2D grid.
-- Thin barrier along diagonal causes partial reflection and tunneling.
-- After scattering: two lobes form an entangled superposition of outcomes.
-- Use conditional slices to verify entanglement.

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

particle(electron, {
    position = { -2 * um },
    momentum = { momentum },
    width = 0.3 * um,
})

particle(electron, {
    position = { 2 * um },
    momentum = { -momentum },
    width = 0.3 * um,
})

interaction {
    type = "contact",
    particles = { 1, 2 },
    strength = energy * 1.8,
    width = 0.02 * um,
}
