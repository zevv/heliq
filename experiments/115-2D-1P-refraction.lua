description("Refraction and Total Reflection", [[
An electron entering a thick slab of higher potential at an angle.
Inside the slab, kinetic energy drops, wavelength increases — the
wave bends (Snell's law for matter waves). At steep angles it
refracts through. At shallow angles (grazing incidence), the normal
component of kinetic energy falls below the barrier and the wave
reflects completely — total internal reflection, same as fiber optics.
Try adjusting the entry angle by editing momentum components.]])

domain {
    { min = -100 * nm, max = 100 * nm, points = 512 },
    { min = -100 * nm, max = 100 * nm, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.1 * eV
local p = math.sqrt(2 * m_electron * energy)

local angle = 0.5
local ca = math.cos(angle)
local sa = math.sin(angle)


-- enter at ~55 degrees from wall normal: just above total reflection threshold
-- try changing 0.55/0.83 to 0.3/0.95 for total reflection
particle(electron, {
    position = { ca * -80 * nm, sa * -80 * nm },
    momentum = { ca * p, sa * p },
    width = 8 * nm,
})

-- thick slab of repulsive potential: acts as a less-dense medium
-- V = 0.5 * energy: reflects at angles > ~45 deg from normal
barrier {
    from = { -25 * nm, -100 * nm },
    to   = { 25 * nm,  100 * nm },
    height = 0.4 * energy,
}
