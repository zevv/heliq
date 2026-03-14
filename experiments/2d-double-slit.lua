-- Double slit experiment - tuned for 512x512 performance
-- 8 points per wavelength, 5x barrier, ~1min to see diffraction

dimensions(2)

domain {
    { min = -5 * um, max = 5 * um, points = 512 },
    { min = -5 * um, max = 5 * um, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.062e-3 * eV   -- 0.062 meV, wavelength ~156nm, 8 pts/wavelength
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -2 * um, 0 },
    momentum = { momentum, 0 },
    width = 0.2 * um,
})

-- double slit: two gaps of 300nm, separated by 400nm center-to-center
-- slits at y = [-0.35, -0.05] and [0.05, 0.35] um, each 300nm wide
local wall_h = 5 * energy     -- 5x kinetic energy
local wall_w = 0.2 * um       -- thick wall, ~25 grid points

barrier {
    from = { -wall_w, -10 * um },
    to   = {  wall_w, -0.35 * um },
    height = wall_h,
}
barrier {
    from = { -wall_w, -0.15 * um },
    to   = {  wall_w,  0.15 * um },
    height = wall_h,
}
barrier {
    from = { -wall_w,  0.35 * um },
    to   = {  wall_w,  10 * um },
    height = wall_h,
}
