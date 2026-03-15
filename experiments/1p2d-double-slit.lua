-- Double slit experiment: the quintessential quantum interference demo.
-- Two 300nm slits separated by 400nm center-to-center.
-- Watch the interference fringes develop on the far side of the barrier.

domain {
    { min = -5 * um, max = 5 * um, points = 512 },
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.062e-3 * eV
local momentum = 0.5 * math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -2 * um, 0 },
    momentum = { momentum, 0 },
    width = 0.25 * um,
})

local wall_h = 5 * energy
local wall_w = 0.1 * um

barrier {
    from = { -wall_w, -10 * um },
    to   = {  wall_w, -0.35 * um },
    height = wall_h,
}

barrier {
    from = { -wall_w, -0.15 * um },
    to   = {  wall_w,  0.15 * um },
    height = wall_h,
}

barrier {
    from = { -wall_w,  0.35 * um },
    to   = {  wall_w,  10 * um },
    height = wall_h,
}
