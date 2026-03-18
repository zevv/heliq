description("2D Which-Path (WIP)", [[
Two particles in 2D — 4D config space (64^4).

Step 2: A splits at beam splitter. Contact interaction with B.
Only the transmitted component reaches B and kicks it.
B should end up in a superposition of kicked/not-kicked.]])

local W = 10 * um
local H = 2.5 * um

domain {
    { min = -W, max = W, points = 128 },  -- x_A
    { min = -H, max = H, points = 32 },  -- y_A
    { min = -W, max = W, points = 128 },  -- x_B
    { min = -H, max = H, points = 32 },  -- y_B
}

electron = def_particle {
    mass   = m_electron,
    charge = -e_charge,
}

local energy   = 6e-6 * eV
local momentum = math.sqrt(2 * m_electron * energy)

-- particle A: moving right toward beam splitter
particle(electron, {
    position = { -3 * um, 0 },
    momentum = { momentum, 0 },
    width    = 0.4 * um,
})

-- particle B: stationary, out of the way for now
particle(electron, {
    position = { 1 * um, 0 },
    momentum = { 0, 0 },
    width    = 0.4 * um,
})

-- beam splitter: vertical barrier at x_A = 0, spans all y_A and all B space
local big = 2 * H
barrier {
    from   = { -2 * um + -0.05 * um, -H, -big, -big },
    to     = { -2 * um +  0.05 * um,  H,  big,  big },
    height = energy * 0.7,
}

-- which-path interaction: fires when A's transmitted component reaches B
interaction {
    type      = "contact",
    particles = { 1, 2 },
    strength  = energy * 5,
    width     = 0.5 * um,
}
