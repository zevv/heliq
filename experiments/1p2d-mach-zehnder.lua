-- Beam splitter and mirror: quantum interference demo.
-- Particle moves right, hits a thin barrier (beam splitter).
-- Transmitted part continues right. Reflected part bounces off a mirror.
-- Reflected part returns and hits the splitter again — self-interference.
-- Enable absorbing boundary (B key) to clean up edge reflections.

local L = 5 * um

domain {
    { min = -L, max = L, points = 512 },
    { min = -L, max = L, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.1e-3 * eV
local p = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -1 * um, 0 },
    momentum = { p, 0 },
    width = 0.3 * um,
})

-- beam splitter: thin barrier at x = 1um
local split_h = energy * 0.9
local split_w = 0.02 * um

barrier {
    from = { 1 * um - split_w, -L },
    to   = { 1 * um + split_w,  L },
    height = split_h,
}

-- mirror: thick wall at x = -3um
local wall_h = energy * 200
local wall_w = 0.15 * um

barrier {
    from = { -3 * um - wall_w, -L },
    to   = { -3 * um + wall_w,  L },
    height = wall_h,
}
