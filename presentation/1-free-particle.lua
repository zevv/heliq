description("Moving Electron", [[
A real electron at 0.1 eV — the de Broglie wave.
The helix shows Re(ψ) and Im(ψ) spiraling as the packet moves.
Watch it disperse: faster momentum components outrun slower ones.
The domain is 200 nm — a nanostructure scale.
Toggle momentum view to see the momentum distribution stays fixed.
Reverse time with / — the packet refocuses perfectly.]])

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

