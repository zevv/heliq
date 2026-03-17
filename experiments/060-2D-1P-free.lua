description("Moving Electron 2D", [[
A Gaussian wavepacket with momentum in 2D — the 2D version of 010.
The grid view shows |ψ|² as brightness. The helix views show
1D slices through the 2D wavefunction at the cursor position.
Drag the cursor to explore.]])

domain {
    { min = -100 * nm, max = 100 * nm, points = 512 },
    { min = -100 * nm, max = 100 * nm, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.1 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -40 * nm, -10 * nm },
    momentum = { momentum * 0.7, momentum * 0.7 },
    width = 5 * nm,
})
