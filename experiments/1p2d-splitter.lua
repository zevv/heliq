-- Beam splitter cavity in 2D.
-- Particle moves diagonally, bounces between mirrors and splitters.
-- Splitters at x=0 (with gap) partially transmit/reflect.
-- Mirrors at x=±3um fully reflect.
-- Watch for interference patterns from multiple passes through the splitters.

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
    position = { -1 * um, -4 * um },
    momentum = { p, p },
    width = 0.3 * um,
})

-- beam splitter: thin barrier at x = 1um
local split_h = energy * 0.9
local split_w = 0.02 * um

-- Top splitter
barrier {
    from = { -split_w,  1 * um },
    to   = {  split_w,  5 * um },
    height = split_h,
}

-- Bottom splitter
barrier {
    from = { -split_w, -5 * um },
    to   = {  split_w, -1 * um },
    height = split_h,
}

local wall_h = energy * 20
local wall_w = 0.15 * um

-- left mirror

barrier {
    from = { -3 * um - wall_w, -2 * um},
    to   = { -3 * um + wall_w,  2 * um},
    height = wall_h,
}

-- right mirror

barrier {
	from = { 3 * um - wall_w, -2 * um},
	to   = { 3 * um + wall_w,  2 * um},
	height = wall_h,
}
