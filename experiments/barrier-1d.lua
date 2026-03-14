-- Single electron hitting a potential barrier in 1D

dimensions(1)

domain {
    { min = -5 * um, max = 5 * um, points = 1024 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.001 * eV   -- 1 meV, wavelength ~39nm, ~4 pts per wavelength
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -2 * um },
    momentum = { momentum },
    width = 0.2 * um,
})

barrier {
    from = { -50 * nm },
    to   = {  50 * nm },
    height = 0.005 * eV,    -- 5 meV, higher than kinetic energy
}
