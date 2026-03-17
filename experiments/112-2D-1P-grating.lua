description("Diffraction Grating", [[
30-slit grating with pitch near the de Broglie wavelength (lambda ~3.9 nm).
At this spacing the first diffraction order goes to near-grazing angles
and most energy reflects — this is effectively a reflection grating, or
equivalently, Bragg diffraction off a 1D crystal lattice.

Compare with 110 (double slit): two slits give broad fringes; many slits
produce narrow, bright principal maxima. The high barrier makes the walls
nearly opaque, so transmission is weak and the reflected diffraction
pattern dominates. Each slit radiates its own wavelet; in the near field
(visible here) these appear as parallel beamlets at the diffraction angle.

Try: lower wall_h to ~10*energy to see more transmission.
     change pitch to 8nm for clear transmitted orders at ~29 degrees.
     set pitch below 3.9nm — all orders become evanescent, only
     zeroth order (straight-through) survives.]])

domain {
    { min = -100 * nm, max = 100 * nm, points = 512 },
    { min = -100 * nm, max = 100 * nm, points = 512 },
}

electron = def_particle {
    mass   = m_electron,
    charge = -e_charge,
}

local energy   = 0.1 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -40 * nm, 0 },
    momentum = { momentum, 0 },
    width    = 15 * nm,
})

local N       = 30          -- number of slits
local slit_w  = 1 * nm     -- half-width of each slit opening
local pitch   = 5 * nm     -- centre-to-centre slit spacing
local wall_h  = 100.0 * energy

-- grating spans from y_min to y_max; one barrier segment per gap
local y_min = -100 * nm
local y_max =  100 * nm

-- bottom edge cap
local first_center = -(N - 1) / 2 * pitch
barrier {
    from   = { -1 * nm, y_min },
    to     = {  1 * nm, first_center - slit_w },
    height = wall_h,
}

-- inter-slit walls
for i = 0, N - 2 do
    local c1 = first_center + i * pitch
    local c2 = c1 + pitch
    barrier {
        from   = { -1 * nm, c1 + slit_w },
        to     = {  1 * nm, c2 - slit_w },
        height = wall_h,
    }
end

-- top edge cap
local last_center = first_center + (N - 1) * pitch
barrier {
    from   = { -1 * nm, last_center + slit_w },
    to     = {  1 * nm, y_max },
    height = wall_h,
}
