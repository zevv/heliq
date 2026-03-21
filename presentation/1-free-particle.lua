description("Moving Electron", [[
A free electron at 0.1 eV. The wave function is 512 complex numbers
on a line. The helix shows the complex spiral — its wavelength is the
momentum. The envelope is the probability of finding the particle.
Watch it spread as it moves — that's the uncertainty principle at work.]])

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

