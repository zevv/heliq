description("2D Entangling Collision", [[
Two electrons in 2D colliding via a weak contact interaction.
4D config space (128x32x128x32).

Both particles move toward each other along x. The contact interaction
is tuned for partial tunneling — the blob partially passes through,
partially bounces. The two outcomes are correlated: if A bounced,
B bounced too. If A passed through, so did B.

This is the 2D version of 170. The scattering is isotropic in 2D,
so you see angular deflection as well as the forward/backward split.

View x_A vs x_B in the grid to see the diagonal bounce.
Use Measure to collapse and verify correlations.]])

local W = 10 * um
local H = 2.5 * um

domain {
    { min = -W, max = W, points = 128 },  -- x_A
    { min = -H, max = H, points = 32 },   -- y_A
    { min = -W, max = W, points = 128 },  -- x_B
    { min = -H, max = H, points = 32 },   -- y_B
}

electron = def_particle {
    mass   = m_electron,
    charge = -e_charge,
}

local energy   = 6e-6 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -3 * um, 0 },
    momentum = { momentum, 0 },
    width    = 0.4 * um,
})

particle(electron, {
    position = { 3 * um, 0 },
    momentum = { -momentum, 0 },
    width    = 0.4 * um,
})

-- weak contact interaction: partial tunneling
interaction {
    type      = "contact",
    particles = { 1, 2 },
    strength  = energy * 2.8,
    width     = 0.3 * um,
}
