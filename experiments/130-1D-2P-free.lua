description("Two Free Particles", [[
Introduction to configuration space. Two electrons, no interaction,
no momentum — just spreading.

The grid shows (x1, x2): one axis per particle. A single blob
represents the joint state. Because there is no interaction, the
state is a product state: the blob stays circular and each marginal
(helix widget) evolves independently.

This is the baseline before adding momentum (140), barriers (150),
or interactions (160+). Watch the blob spread symmetrically on both
axes at the same rate — identical particles, identical initial width.]])

domain {
    { min = -100 * nm, max = 100 * nm, points = 512 },
    { min = -100 * nm, max = 100 * nm, points = 512 },
}

electron = def_particle {
    mass   = m_electron,
    charge = -e_charge,
}

-- zero momentum: pure spreading, no drift
particle(electron, {
    position = { -30 * nm },
    momentum = { 0 },
    width    = 5 * nm,
})

particle(electron, {
    position = { 30 * nm },
    momentum = { 0 },
    width    = 5 * nm,
})
