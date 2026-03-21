description("Quantum Tunneling", [[
Same wall as (3), but thinner and lower — tuned for 50% transmission.
The wave function splits into two bumps: reflected and transmitted.
Still one electron, one wave function, amplitude in two regions.
This is a half mirror for electrons.]])

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

-- same width as reflection barrier, but lower — particle leaks through
barrier {
    from = { -1 * nm },
    to   = {  1 * nm },
    height = 0.9 * energy,
}
