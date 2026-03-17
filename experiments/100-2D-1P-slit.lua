description("Single Slit Diffraction", [[
An electron passing through a narrow gap in a wall.
The wavefunction diffracts — spreads into a circular wavefront on
the far side. The narrower the slit, the more it spreads. This is
Huygens' principle: every point in the slit acts as a new source.]])

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

local slit_w = 2.5 * nm

barrier {
    from = { -1 * nm,  slit_w },
    to   = {  1 * nm,  100 * nm },
    height = 10 * energy,
}

barrier {
    from = { -1 * nm, -100 * nm },
    to   = {  1 * nm, -slit_w },
    height = 10 * energy,
}
