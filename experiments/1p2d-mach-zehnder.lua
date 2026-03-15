-- Mach-Zehnder interferometer in 2D.
-- Particle enters at 45°, hits the bottom beam splitter and splits.
-- Path A (transmitted) goes right to the right mirror, reflects upward.
-- Path B (reflected) goes left to the left mirror, reflects upward.
-- Both paths meet at the top beam splitter and interfere.
-- The gap between the two splitter halves keeps the paths separate.
-- Output depends on the relative phase accumulated on each path.
-- Try adjusting splitter height to tune the splitting ratio.

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

local split_h = energy * 0.9
local split_w = 0.02 * um
local mirror_h = energy * 20
local mirror_w = 0.15 * um

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

-- left mirror
barrier {
    from = { -3 * um - mirror_w, -2 * um},
    to   = { -3 * um + mirror_w,  2 * um},
    height = mirror_h,
}

-- right mirror
barrier {
    from = { 3 * um - mirror_w, -2 * um},
    to   = { 3 * um + mirror_w,  2 * um},
    height = mirror_h,
}
