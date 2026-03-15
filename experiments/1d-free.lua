-- Free electron in 1D, stationary, no potential

domain {
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

particle(electron, {
    position = { 0 },
    momentum = { 0 },
    width = 0.05 * um,
})

