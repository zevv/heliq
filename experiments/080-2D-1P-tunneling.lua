description("Quantum Tunneling 2D", [[
An electron tunneling through a thin barrier in 2D — the 2D version of 040.
The barrier spans the full y-axis. Watch the circular wavefront split
into reflected and transmitted components. The transmitted wave emerges
as a new circular wavefront on the far side.]])

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
    from = { -0.5 * nm, -100 * nm },
    to   = {  0.5 * nm,  100 * nm },
    height = 1.0 * energy,
}
