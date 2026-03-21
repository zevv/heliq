description("Mach-Zehnder Interferometer", [[
A beam splitter divides the electron into two paths. Mirrors redirect
both paths to a second meeting point where they recombine and interfere.
The output depends on the relative phase accumulated on each path.
This is the optical bench of quantum mechanics — the building block
of quantum eraser experiments.]])

domain {
    { min = -100 * nm, max = 100 * nm, points = 512 },
    { min = -100 * nm, max = 100 * nm, points = 512 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.1 * eV
local p = math.sqrt(2 * m_electron * energy)

-- enter at 45 degrees
particle(electron, {
    position = { -20 * nm, -80 * nm },
    momentum = { p * 0.707, p * 0.707 },
    width = 6 * nm,
})

local split_h = energy * 0.65
local split_w = 0.5 * nm
local mirror_h = energy * 20
local mirror_w = 1 * nm

-- beam splitter (vertical, at x=0)
barrier {
    from = { -split_w, -100 * nm },
    to   = {  split_w,  100 * nm },
    height = split_h,
}

-- left mirror
barrier {
    from = { -50 * nm - mirror_w, -40 * nm },
    to   = { -50 * nm + mirror_w,  40 * nm },
    height = mirror_h,
}

-- right mirror
barrier {
    from = { 50 * nm - mirror_w, -40 * nm },
    to   = { 50 * nm + mirror_w,  40 * nm },
    height = mirror_h,
}
