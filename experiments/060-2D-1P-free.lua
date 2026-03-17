-- Free particle in 2D: a circular Gaussian wavepacket dispersing.
-- Same physics as 1D, but now the spreading is radial.

domain {
    { min = -5 * um, max = 5 * um, points = 512 },
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

particle(electron, {
    position = { 0, 0 },
    momentum = { 0, 0 },
    width = 0.4 * um,
})

