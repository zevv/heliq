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

barrier {
    from = { -10 * nm, -10 * um },
    to   = {  10 * nm,  10 * um },
    height = 0.005 * eV,
}
