description("Alpha Decay", [[
A particle trapped inside a potential well with thin walls — Gamow's
model of alpha decay.

The particle fills the well as a standing wave (zero momentum, wide
Gaussian approximating the ground state). It leaks continuously
through both walls — smooth exponential decay, no bouncing.

This is how real alpha decay works: the alpha particle is in a
quasi-stationary state inside the nucleus, uniformly spread across
the well. The wavefunction leaks through the Coulomb barrier at a
steady rate. The decay probability is exponential — the half-life.

Watch the trace widget (F4): the trapped amplitude shrinks steadily
while symmetric streams leak outward from both walls.

Try: give the particle momentum to see pulsed (bouncing) emission.
     thicker walls for longer half-life.
     higher barrier for much longer half-life.]])

domain {
    { min = -200 * nm, max = 200 * nm, points = 1024 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 1 * eV
local momentum = math.sqrt(2 * m_electron * energy)

-- particle fills the well — zero momentum, wide Gaussian approximates
-- the ground state. No bouncing, smooth continuous leak.
particle(electron, {
    position = { 0 },
    momentum = { 0 },
    width = 6 * nm,
})

-- potential well: two thin walls separated by 40nm
local wall_h = 0.03 * energy
local wall_w = 1.0 * nm
local well_half = 20 * nm

barrier {
    from = { -well_half - wall_w },
    to   = { -well_half + wall_w },
    height = wall_h,
}

barrier {
    from = {  well_half - wall_w },
    to   = {  well_half + wall_w },
    height = wall_h,
}
