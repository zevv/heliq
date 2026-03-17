description("Barrier Reflection 2D", [[
An electron hitting a tall barrier in 2D — the 2D version of 030.
The barrier spans the full y-axis. Watch the circular wavefront
reflect and the standing wave pattern form in front of the wall.]])

domain {
    { min = -100 * nm, max = 100 * nm, points = 512 },
    { min = -100 * nm, max = 100 * nm, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.1 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -30 * nm, 0 },
    momentum = { momentum, 0 },
    width = 5 * nm,
})

barrier {
    from = { 20 * nm, -100 * nm },
    to   = { 30 * nm,  100 * nm },
    height = 10 * energy,
}
