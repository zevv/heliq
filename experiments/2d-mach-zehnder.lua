-- Splitter + mirror: interference demo
-- Particle moves right, hits thin barrier (splitter).
-- Transmitted continues right. Reflected goes back left, hits mirror, returns.
-- Reflected part hits the splitter again — interference with transmitted part.


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

-- splitter: thin barrier at x=1um
local split_h = energy * 0.9
local split_w = 0.02 * um

barrier {
    from = { 1 * um - split_w, -L },
    to   = { 1 * um + split_w,  L },
    height = split_h,
}

-- mirror: thick wall at x = -3um, sends reflected part back
local wall_h = energy * 200
local wall_w = 0.15 * um

barrier {
    from = { -3 * um - wall_w, -L },
    to   = { -3 * um + wall_w,  L },
    height = wall_h,
}
