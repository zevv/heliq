description("Barrier Reflection", [[
Same electron as (1), now hitting a wall. The reflected wave interferes
with the incoming wave — the envelope shows nodes where the particle
cannot be found. Zoom into the barrier edge: the wave leaks in but
decays. It can't make it through — but compare with (4).]])

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

barrier {
    from = { -1 * nm },
    to   = {  1 * nm },
    height = 3 * energy,
}
