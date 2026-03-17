description("Double Slit", [[
The quintessential quantum experiment. An electron passes through
two slits simultaneously and interferes with itself on the far side.
The interference fringes prove the electron went through both slits.
Compare with 100 (single slit) — one slit gives diffraction,
two slits give interference on top of the diffraction.]])

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
    position = { -30 * nm, 0 },
    momentum = { momentum, 0 },
    width = 10 * nm,
})

local slit_w = 2 * nm    -- slit half-width
local slit_d = 8 * nm    -- slit center-to-center half-distance

-- three wall segments: below, between, above the slits
barrier {
    from = { -1 * nm, -100 * nm },
    to   = {  1 * nm, -slit_d - slit_w },
    height = 10 * energy,
}

barrier {
    from = { -1 * nm, -slit_d + slit_w },
    to   = {  1 * nm,  slit_d - slit_w },
    height = 10 * energy,
}

barrier {
    from = { -1 * nm,  slit_d + slit_w },
    to   = {  1 * nm,  100 * nm },
    height = 10 * energy,
}
