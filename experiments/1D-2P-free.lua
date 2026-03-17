


domain {
    { min = -5 * um, max = 5 * um, points = 512 },
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

particle(electron, {
    position = { -1.5 * um },
    momentum = { 0 },
    width = 0.4 * um,
})

particle(electron, {
    position = { 1.5 * um },
    momentum = { 0 },
    width = 0.4 * um,
})

