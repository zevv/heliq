description("Barrier Reflection", [[
An electron hitting a tall potential barrier — 10x the kinetic energy.
Mostly reflects, but watch for the evanescent tail leaking into the barrier.
As the reflected wave returns, it interferes with the incoming wave,
creating a standing wave pattern. This is the quantum version of a ball
bouncing off a wall — except the ball partially leaks through.]])

domain {
    { min = -100 * nm, max = 100 * nm, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.1 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -30 * nm },
    momentum = { momentum },
    width = 5 * nm,
})

barrier {
    from = { -1 * nm },
    to   = {  1 * nm },
    height = 10 * energy,
}
