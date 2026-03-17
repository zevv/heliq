-- Two particles with different widths and hard bounce.
-- Asymmetry makes entanglement from the bounce visible.
-- Blob rotates through diagonal orientations during reflection.
-- Finite interaction time creates phase shearing.
-- Use conditional slices to verify: if slices differ, it's entangled.

domain {
    { min = -5 * um, max = 5 * um, points = 512 },
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.01-3 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -2 * um },
    momentum = { momentum },
    width = 0.4 * um,
})

particle(electron, {
    position = { 2 * um },
    momentum = { -momentum },
    width = 0.4 * um,
})

interaction {
    type = "contact",
    particles = { 1, 2 },
    strength = energy * 20,
    width = 0.02 * um,
}
