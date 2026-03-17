-- Two independent particles, each hitting their own barrier.
-- No interaction: product state, no entanglement.
-- Config space shows 4 blobs (all combinations of reflected/transmitted).
-- Measuring one particle does NOT affect the other.

domain {
    { min = -5 * um, max = 5 * um, points = 512 },
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.062e-3 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -3.5 * um },
    momentum = { momentum },
    width = 0.3 * um,
})

particle(electron, {
    position = { 3.5 * um },
    momentum = { -momentum },
    width = 0.3 * um,
})

local wall_h = energy * 1.5
local wall_w = 0.02 * um

barrier {
    from = { -2 * um - wall_w, -5 * um },
    to   = { -2 * um + wall_w,  5 * um },
    height = wall_h,
}

barrier {
    from = { -5 * um, -2 * um - wall_w },
    to   = {  5 * um, -2 * um + wall_w },
    height = wall_h,
}
