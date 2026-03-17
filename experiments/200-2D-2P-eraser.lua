description("2D Quantum Eraser", [[
Two particles in 2D physical space — 4D configuration space (64^4).

Particle A moves toward a double slit. Particle B (heavy detector)
sits near the upper slit. When A passes through that slit, the
contact interaction kicks B, encoding which-path information.

This is the full quantum eraser setup in 2D. The heavy detector
records which slit A used without blocking it — a non-destructive
measurement that still destroys interference.

Note: this experiment uses um scale and ueV energy because the
64-point resolution per axis requires longer wavelengths to satisfy
Nyquist. The physics is identical to the nm-scale experiments.]])

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

-- heavy "detector" particle — barely moves when kicked, but records the interaction
detector = def_particle {
    mass = m_electron * 100,
    charge = -e_charge,
}

-- ~3 ueV energy, 44% Nyquist at 64 points
local energy = 3e-6 * eV
local momentum = math.sqrt(2 * m_electron * energy)

-- particle A: moving right toward the double slit
particle(electron, {
    position = { -3 * um, 0 },
    momentum = { momentum, 0 },
    width = 0.6 * um,
})

-- particle B: heavy detector, near the upper slit
-- records which-path info in its momentum without blocking A
particle(detector, {
    position = { 0, 2.2 * um },
    momentum = { 0, 0 },
    width = 0.6 * um,
})

-- double slit wall: barrier at x=0 with two gaps
-- wall spans full y, with two slits cut out
-- slit centers at y = +0.8um and y = -0.8um, each 0.6um wide
local wall_x = 0
local wall_w = 0.3 * um
local slit_w = 0.3 * um
local slit_sep = 1.2 * um   -- center-to-center

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

-- which-path marker: Gaussian contact interaction
-- smooth falloff — A passes through with a phase shift, not a hard block
-- B (heavy) barely moves but records which-path in its momentum
interaction {
    type = "contact",
    particles = { 1, 2 },
    strength = energy * 40,
    width = 0.5 * um,
}
