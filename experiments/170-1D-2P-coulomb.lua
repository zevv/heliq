-- Two particles with broad Coulomb interaction.
-- The broad potential creates differential phase across the wavepacket.
-- Entanglement emerges even without tunneling: the interaction is smooth and weak.
-- Watch the blob distort and develop correlations as particles pass.

domain {
    { min = -5 * um, max = 5 * um, points = 512 },
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.062e-3 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -2 * um },
    momentum = { momentum },
    width = 0.3 * um,
})

particle(electron, {
    position = { 2 * um },
    momentum = { -momentum },
    width = 0.3 * um,
})

interaction {
    type = "coulomb",
    particles = { 1, 2 },
    softening = 1.3 * um,
}
