description("Mach-Zehnder Which-Path", [[
Two particles in 2D — 4D configuration space (128x128x16x16).

Same Mach-Zehnder interferometer as 121, but with a heavy detector
particle sitting on the left arm between BS1 and the left mirror.

Without interaction: electron splits, reflects, recombines — output
goes preferentially to one port (right side), same as 1P version.

With interaction: the left arm kicks the detector via contact
interaction, entangling them. The which-path information encoded in
the detector's momentum destroys the interference at recombination.
The output should become more symmetric between ports.

Measure the detector (M on its axis) to collapse the which-path
info and see the correlation with the electron's output port.]])

domain {
    { min = -4.0 * um, max =  4.0 * um, points = 128 },  -- x_A
    { min = -5.0 * um, max =  6.5 * um, points = 128 },  -- y_A
    { min = -3.0 * um, max = -1.0 * um, points =  16 },   -- x_B
    { min = -1.0 * um, max =  1.0 * um, points =  16 },   -- y_B
}

electron = def_particle {
    mass   = m_electron,
    charge = -e_charge,
}

detector = def_particle {
    mass   = m_electron * 3,
    charge = -e_charge,
}

local energy   = 10e-6 * eV
local momentum = math.sqrt(2 * m_electron * energy)

-- particle A: electron, 45 degree entry (identical to 1P version)
particle(electron, {
    position = { -1.2 * um, -3.4 * um },
    momentum = { momentum * 0.707, momentum * 0.707 },
    width    = { 0.4 * um, 0.6 * um },
})

-- particle B: heavy detector, stationary, on the left arm path
particle(detector, {
    position = { -2.0 * um, 0 },
    momentum = { 0, 0 },
    width    = 0.4 * um,
})

-- all barriers span full detector domain (axes 3-4)
local db = 3 * um  -- comfortably larger than detector domain

-- barrier tuning (same as 1P)
local split_h  = energy * 1.2
local split_w  = 0.05 * um

-- BS1: vertical beam splitter at x = 0, spans full y
barrier {
    from = { -split_w, -5 * um,   -db, -db },
    to   = {  split_w,  6.5 * um,  db,  db },
    height = split_h,
}

local mirror_h = energy * 50
local mirror_w = 0.15 * um

-- left mirror
barrier {
    from = { -2.2 * um - mirror_w, -2.5 * um,  -db, -db },
    to   = { -2.1 * um + mirror_w,  2.5 * um,   db,  db },
    height = mirror_h,
}

-- right mirror
barrier {
    from = { 2.1 * um - mirror_w, -2.5 * um,  -db, -db },
    to   = { 2.2 * um + mirror_w,  2.5 * um,   db,  db },
    height = mirror_h,
}

-- which-path interaction: short-range 1/r^3 potential
-- power>1 bypasses k_coulomb*charges; strength is the peak V at r=0
-- falls off fast: only the near arm feels it, far arm sees ~nothing
interaction {
    type      = "coulomb",
    particles = { 1, 2 },
    softening = 1.3 * um,
    strength  = energy * 0.1,
    power     = 4,
}
