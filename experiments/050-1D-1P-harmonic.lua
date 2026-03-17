description("Harmonic Oscillator", [[
An electron in a parabolic potential, displaced from center.
The wavepacket width is matched to the ground state — it oscillates
back and forth as a coherent state without dispersing. This is the
quantum analog of a pendulum. The period is about 1.4 ps.
Compare with 020 (free): there, the packet spreads. Here the well
holds it together.]])

domain {
    { min = -100 * nm, max = 100 * nm, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

-- width matched to ground state: sigma = sqrt(hbar / (m * omega))
-- so k = hbar^2 / (m * sigma^4)
local sigma = 5 * nm
local k = hbar * hbar / (m_electron * sigma^4)

particle(electron, {
    position = { 15 * nm },
    momentum = { 0 },
    width = sigma,
})

harmonic {
    center = { 0 },
    k = k,
}
