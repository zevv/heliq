description("Harmonic Oscillator 2D", [[
An electron orbiting in a 2D harmonic trap — the 2D version of 050.
Displaced from center with tangential momentum, it traces a circular
orbit as a coherent state without dispersing. The width is matched
to the ground state. This is the quantum analog of an orbiting planet.]])

domain {
    { min = -100 * nm, max = 100 * nm, points = 512 },
    { min = -100 * nm, max = 100 * nm, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local sigma = 5 * nm
local k = hbar * hbar / (m_electron * sigma^4)
local omega = math.sqrt(k / m_electron)
local r0 = 30 * nm
local p0 = m_electron * omega * r0

particle(electron, {
    position = { r0, 0 },
    momentum = { 0, p0 },
    width = sigma,
})

harmonic {
    center = { 0, 0 },
    k = k,
}
