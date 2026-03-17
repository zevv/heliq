-- Single slit diffraction in 2D.
-- Electron passes through a 400nm gap and diffracts.
-- Watch the circular wave pattern emerge after passing through the slit.

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
    position = { -2 * um, 0 },
    momentum = { momentum, 0 },
    width = 0.2 * um,
})

local wall_h = 5 * energy
local wall_w = 0.2 * um

barrier {
    from = { -wall_w,  0.2 * um },
    to   = {  wall_w,  10 * um },
    height = wall_h,
}

barrier {
    from = { -wall_w, -10 * um },
    to   = {  wall_w, -0.2 * um },
    height = wall_h,
}
