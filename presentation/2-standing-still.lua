description("Stationary Wavepacket", [[
Zero momentum — the particle knows where it is but can't stay still.
The uncertainty principle forces a spread in momentum, and each component
flies apart. Same physics as (1), just without the drift.]])

domain {
    { min = -100 * nm, max = 100 * nm, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

particle(electron, {
    position = { 0 },
    momentum = { 0 },
    width = 5 * nm,
})
