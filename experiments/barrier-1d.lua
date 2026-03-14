-- Single electron hitting a potential barrier in 1D

dimensions(1)

domain {
    { min = -5 * um, max = 5 * um, points = 1024 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 3 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -2 * um },
    momentum = { momentum },
    width = 0.5 * um,
})

barrier {
    from = { -50 * nm },
    to   = {  50 * nm },
    height = 5 * eV,
}
