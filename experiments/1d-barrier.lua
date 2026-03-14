-- Single electron hitting a potential barrier in 1D

dimensions(1)

domain {
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.062e-3 * eV   -- 0.062 meV, ~8 pts/wavelength at 512
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -2 * um },
    momentum = { momentum },
    width = 0.2 * um,
})

local wall_h = 5 * energy
local wall_w = 0.2 * um

barrier {
    from = { -wall_w },
    to   = {  wall_w },
    height = wall_h,
}
