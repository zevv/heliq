description("Two Particles with Momentum", [[
Two electrons moving toward each other in 1D — no interaction.

In config space the joint blob moves diagonally: particle 1 drifts
right (x1 increases), particle 2 drifts left (x2 decreases), so the
blob tracks the anti-diagonal. Purely kinematic.

When the marginals overlap in real space nothing happens — no
interaction means they pass straight through each other. The blob
crosses the x1=x2 diagonal unimpeded.

Compare with 160 (contact): same setup, but the diagonal becomes a
wall the blob bounces off. That bounce is the collision.]])

domain {
    { min = -100 * nm, max = 100 * nm, points = 512 },
    { min = -100 * nm, max = 100 * nm, points = 512 },
}

electron = def_particle {
    mass   = m_electron,
    charge = -e_charge,
}

local energy   = 0.1 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -30 * nm },
    momentum = { momentum },
    width    = 5 * nm,
})

particle(electron, {
    position = { 30 * nm },
    momentum = { -momentum },
    width    = 5 * nm,
})
