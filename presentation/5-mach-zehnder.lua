description("Mach-Zehnder Interferometer", [[
Now in 2D. The beam splitter from (4) divides the electron into two
paths. Mirrors redirect both halves back to the splitter where they
recombine. The phases line up: everything exits one port, nothing
exits the other. The two halves were never apart. Compare with (7)
to see what happens when something watches.]])

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
