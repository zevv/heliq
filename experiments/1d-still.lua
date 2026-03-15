-- Single electron diffracting through a slit in 2D


domain {
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.0002e-3 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { 0 },
    momentum = { 0 },
    width = 0.05 * um,
})

