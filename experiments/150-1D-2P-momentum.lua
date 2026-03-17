-- Two particles with momentum in 1D: watching config space dynamics.
-- Particle 1 moves right, particle 2 moves left.
-- In config space, the blob moves diagonally.
-- No interaction: they pass through each other.

domain {
    { min = -5 * um, max = 5 * um, points = 512 },
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.03e-3 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -1.5 * um },
    momentum = { momentum },
    width = 0.4 * um,
})

particle(electron, {
    position = { 1.5 * um },
    momentum = { -momentum },
    width = 0.4 * um,
})
