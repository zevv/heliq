description("Two Independent Barriers", [[
Two electrons, each hitting their own tunneling barrier. No interaction.

The barriers are axis-aligned in config space: one along x1, one along
x2. Each particle tunnels independently — the state stays a product
state throughout. After tunneling, config space splits into four blobs:
all combinations of transmitted/reflected for each particle.

Particle 1 hits its barrier first — watch the blob split into two.
Then particle 2 hits its barrier — each blob splits again, giving four.
The sequential splitting makes clear that these are independent events.

Key point: measuring particle 1 (M key) collapses x1 and picks one
column of blobs, leaving particle 2 unaffected. This is the baseline
for understanding what entanglement is NOT.

Compare with 160: add an interaction along the x1=x2 diagonal and
the outcomes become correlated — that is entanglement.]])

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

-- particle 1 moves right, particle 2 moves left
particle(electron, {
    position = { -20 * nm },
    momentum = { momentum },
    width    = 5 * nm,
})

particle(electron, {
    position = { -60 * nm },
    momentum = { momentum },
    width    = 5 * nm,
})

-- staggered barriers: particle 1 hits first, then particle 2
-- watch the blob split into 2, then into 4
local wall_h = energy * 1.3
local wall_w = 0.5 * nm

-- barrier for particle 1 at x1 = -30nm (close, hits early)
barrier {
    from = { -0 * nm - wall_w, -100 * nm },
    to   = { -0 * nm + wall_w,  100 * nm },
    height = wall_h,
}

-- barrier for particle 2 at x2 = 15nm (far, hits later)
barrier {
    from = { -100 * nm, -15 * nm - wall_w },
    to   = {  100 * nm, -15 * nm + wall_w },
    height = wall_h,
}
