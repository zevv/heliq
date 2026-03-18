description("Entangling Collision", [[
Two electrons with a weak contact interaction — partial tunneling.

Unlike 160 (hard bounce), the interaction strength is tuned so the
blob partially tunnels through the diagonal and partially reflects.
After the collision, two lobes exist: one where both particles
bounced, one where both passed through.

This is entanglement: the outcomes are correlated. If you measure
particle 1 on the left (bounced), particle 2 must be on the right.
If particle 1 is on the right (passed through), particle 2 is on
the left. The particles' fates are linked — neither has a definite
position until measured.

Verify: in Slice mode, move the cursor along x2. The x1 slice
changes shape depending on where you look. That conditional
dependence is the signature of entanglement.]])

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

-- weak interaction: partial tunneling
interaction {
    type      = "contact",
    particles = { 1, 2 },
    strength  = energy * 1.8,
    width     = 1 * nm,
}
