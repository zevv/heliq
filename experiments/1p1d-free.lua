-- Free particle: a Gaussian wavepacket with no potential.
-- Demonstrates dispersion: the packet spreads over time because
-- different momentum components travel at different speeds.
-- Switch to momentum view to see: position spreads, momentum stays fixed.
-- Reverse time with / — the packet refocuses. Perfectly reversible.

domain {
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

particle(electron, {
    position = { 0 },
    momentum = { 0 },
    width = 0.4 * um,
})
