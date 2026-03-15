-- Moving Gaussian wavepacket traveling rightward in 1D.
-- Watch the packet translate across the domain while dispersing.
-- Switch to momentum view: momentum distribution stays centered but spreads.

domain {
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.02e-3 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { 0 },
    momentum = { momentum },
    width = 0.05 * um,
})
