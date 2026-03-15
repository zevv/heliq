-- Quantum tunneling through a thin barrier in 1D.
-- Barrier height equals kinetic energy: significant transmission.
-- Watch the packet split: reflected and transmitted parts separate cleanly.

domain {
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
    width = 0.5 * um,
})

local wall_h = 1 * energy
local wall_w = 0.05 * um

barrier {
    from = { -wall_w },
    to   = {  wall_w },
    height = wall_h,
}
