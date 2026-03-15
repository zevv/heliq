-- Electron orbiting in a 2D harmonic trap.
-- Coherent state: packet circles the origin without dispersing.
-- Watch it trace a perfect circular orbit in real space.

domain {
    { min = -5 * um, max = 5 * um, points = 512 },
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local sigma = 0.5 * um
local k = hbar * hbar / (m_electron * sigma^4)
local omega = math.sqrt(k / m_electron)
local r0 = 2 * um
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
