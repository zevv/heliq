description("Quantum Tunneling", [[
An electron hitting a thin barrier slightly above its kinetic energy.
Classically, it would bounce. Quantum mechanically, part of the
wavefunction leaks through — tunneling. Watch the packet split into
reflected and transmitted components. The thinner the barrier, the
more gets through. This is how tunnel diodes and STM microscopes work.]])

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
