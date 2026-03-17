-- Two free particles in 1D: introduction to configuration space.
-- The 2D grid shows (x₁, x₂) — one axis per particle.
-- A single blob represents the joint state of both particles.
-- No interaction: the blob is a product state, each marginal independent.

domain {
    { min = -5 * um, max = 5 * um, points = 512 },
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

particle(electron, {
    position = { -1.5 * um },
    momentum = { 0 },
    width = 0.4 * um,
})

particle(electron, {
    position = { 1.5 * um },
    momentum = { 0 },
    width = 0.4 * um,
})

