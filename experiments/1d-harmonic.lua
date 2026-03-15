-- Single electron in a 1D harmonic trap
-- Width matched to ground state: packet stays stationary

domain {
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local sigma = 0.2 * um
local k = hbar * hbar / (m_electron * sigma^4)

particle(electron, {
    position = { 6e-7 },
    momentum = { 0 },
    width = sigma,
})

harmonic {
    center = { 0 },
    k = k,
}
