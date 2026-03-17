



domain {
    { min = -5 * um, max = 5 * um, points = 512 },
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.03-3 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -1.5 * um },
    momentum = { momentum },
    width = 0.4 * um,
})

