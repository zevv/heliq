-- 2D Quantum Eraser: double slit with which-path detection
--
-- Two particles in 2D physical space = 4D config space at 64^4
--
-- Particle A moves rightward toward a double slit.
-- Particle B sits near one slit, contact-coupled to A.
-- When A passes through that slit, B gets kicked → which-path info.
-- Interference pattern on the far side of the slits dies.
--
-- Protocol:
--   1. Run as-is: no interference (which-path info in B)
--   2. Reload (R), decohere B before A reaches screen: interference returns
--   3. Reload (R), measure B: A collapses to one-slit pattern

local L = 5 * um

-- 4D config space: (x_A, y_A, x_B, y_B)
domain {
    { min = -L, max = L, points = 64 },  -- x_A
    { min = -L, max = L, points = 64 },  -- y_A
    { min = -L, max = L, points = 64 },  -- x_B
    { min = -L, max = L, points = 64 },  -- y_B
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

-- ~1 ueV energy, well within Nyquist at 64 points
local energy = 1e-6 * eV
local momentum = math.sqrt(2 * m_electron * energy)

-- particle A: moving right toward the double slit
particle(electron, {
    position = { -3 * um, 0 },
    momentum = { momentum, 0 },
    width = 1.0 * um,
})

-- particle B: stationary, sitting near the upper slit
-- close enough to interact when A passes through that slit
particle(electron, {
    position = { 0, 1.2 * um },
    momentum = { 0, 0 },
    width = 0.8 * um,
})

-- double slit wall: barrier at x=0 with two gaps
-- wall spans full y, with two slits cut out
-- slit centers at y = +0.8um and y = -0.8um, each 0.6um wide
local wall_x = 0
local wall_w = 0.3 * um
local slit_w = 0.6 * um
local slit_sep = 1.6 * um   -- center-to-center

-- barrier must span all of B's config space (x_B, y_B)
-- three wall segments: below bottom slit, between slits, above top slit
local big = 2 * L  -- span full B domain

-- bottom wall: y_A from -L to bottom slit edge
barrier {
    from = { wall_x - wall_w/2, -L,   -big, -big },
    to   = { wall_x + wall_w/2, -slit_sep/2 - slit_w/2,  big, big },
    height = energy * 50,
}

-- middle wall: between the two slits
barrier {
    from = { wall_x - wall_w/2, -slit_sep/2 + slit_w/2,  -big, -big },
    to   = { wall_x + wall_w/2,  slit_sep/2 - slit_w/2,   big, big },
    height = energy * 50,
}

-- top wall: y_A from top slit edge to +L
barrier {
    from = { wall_x - wall_w/2,  slit_sep/2 + slit_w/2,  -big, -big },
    to   = { wall_x + wall_w/2,  L,   big, big },
    height = energy * 50,
}

-- which-path marker: contact interaction between A and B
-- fires when A is near B (near the upper slit)
interaction {
    type = "contact",
    particles = { 1, 2 },
    strength = energy * 3,
    width = 0.5 * um,
}
