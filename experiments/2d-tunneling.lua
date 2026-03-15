-- Single electron hitting a potential barrier in 2D

dimensions(2)

domain {
    { min = -5 * um, max = 5 * um, points = 512 },
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.062e-3 * eV   -- 0.062 meV, ~8 pts/wavelength at 512
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -2 * um, 0 },
    momentum = { momentum, 0 },
    width = 0.5 * um,
})

local wall_h = 1 * energy
local wall_w = 0.05 * um

barrier {
    from = { -wall_w, -10 * um },
    to   = {  wall_w,  10 * um },
    height = wall_h,
}
