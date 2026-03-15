-- Test fixture: 1D barrier at center

domain {
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.01e-3 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { 0 },
    momentum = { momentum },
    width = 0.2 * um,
})

barrier {
    from = { -0.1 * um },
    to   = {  0.1 * um },
    height = 5 * energy,
}
