-- Electron hitting a high potential barrier in 1D.
-- Barrier is 10× the kinetic energy: mostly reflects, tiny evanescent tail.
-- Watch for partial reflection and the standing wave pattern from interference.

domain {
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.01e-3 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { 0 },
    momentum = { momentum },
    width = 0.4 * um,
})

local wall_h = 10 * energy
local wall_w = 0.2 * um

barrier {
    from = { 4 * um },
    to   = { 5 * um },
    height = wall_h,
}
