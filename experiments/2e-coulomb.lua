-- Two electrons in 1D with Coulomb repulsion
-- Joint configuration space: ψ(x₁, x₂) on a 2D grid
-- x-axis = electron 1 position, y-axis = electron 2 position

dimensions(2)

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
    width = 0.1 * um,
})

-- electron 2: moving left from the right
particle(electron, {
    position = { 2 * um },
    momentum = { -momentum },
    width = 0.3 * um,
})

-- Coulomb repulsion between the two electrons
interaction {
    type = "coulomb",
    particles = { 1, 2 },
    softening = 0.1 * um,
}
