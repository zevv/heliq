description("Stationary Wavepacket", [[
A Gaussian wavepacket at rest — zero momentum.
It just sits there and spreads. This is pure dispersion: the uncertainty
principle guarantees a spread in momentum, and each component travels
at a different speed. The narrower the packet, the faster it disperses.
Compare with 010 — same physics, just no drift.]])

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
