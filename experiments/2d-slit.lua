-- Single electron hitting a potential barrier in 2D

dimensions(2)

domain {
    { min = -5 * um, max = 5 * um, points = 1024 },
    { min = -5 * um, max = 5 * um, points = 1024 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.001 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -2 * um, 0 },
    momentum = { momentum, 0 },
    width = 0.2 * um,
})

-- wall with slit: gap of 400nm centered at y=0
local wall_h = 0.1 * eV   -- 100x kinetic energy, minimal tunneling
local wall_w = 250 * nm   -- thick enough for ~50 grid points
barrier {
    from = { -wall_w,  0.1 * um },
    to   = {  wall_w,  10 * um },
    height = wall_h,
}
barrier {
    from = { -wall_w, -10 * um },
    to   = {  wall_w, -0.1 * um },
    height = wall_h,
}

