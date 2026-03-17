description("Contact Interaction", [[
Two electrons colliding via a short-range (contact) interaction.

In config space, the interaction lives on the diagonal x1=x2 — a
repulsive wall. The blob approaches the diagonal and bounces off.
That bounce IS the collision: momentum transfers from one particle
to the other.

After the collision the blob is no longer a simple product state.
Verify: switch to Slice mode, move the cursor along x2 — the shape
of the x1 slice changes depending on where you look. That conditional
dependence is entanglement.

Compare with 140: same particles, same momentum, but no interaction.
The blob crosses the diagonal unimpeded and the marginals stay
independent.]])

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

interaction {
    type      = "contact",
    particles = { 1, 2 },
    strength  = energy * 200,
    width     = 4 * nm,
}
