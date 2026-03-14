-- Single electron through a slit in 1D
-- The "slit" is a gap in a barrier wall

dimensions(1)

domain {
    { min = -5 * um, max = 5 * um, points = 1024 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.001 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -2 * um },
    momentum = { momentum },
    width = 0.3 * um,
})

-- wall with a gap in the middle
-- left wall
barrier {
    from = { -5 * um },
    to   = { -30 * nm },
    height = 0.01 * eV,
}
-- right wall
barrier {
    from = { 30 * nm },
    to   = { 5 * um },
    height = 0.01 * eV,
}
-- gap is [-30nm, 30nm] = 60nm wide slit
