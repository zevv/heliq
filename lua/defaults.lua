-- Post-processing: validation, defaults computation, diagnostic dump.
-- Operates on the populated world table after the user script has run.

local function build(C)

local hbar = C.hbar

local function humanize(v)
    local prefixes = {
        { 1e12, "T" }, { 1e9, "G" }, { 1e6, "M" }, { 1e3, "k" },
        { 1, "" },
        { 1e-3, "m" }, { 1e-6, "u" }, { 1e-9, "n" },
        { 1e-12, "p" }, { 1e-15, "f" }, { 1e-18, "a" },
    }
    local av = math.abs(v)
    for _, p in ipairs(prefixes) do
        if av >= p[1] * 0.99 then
            return string.format("%.4g %s", v / p[1], p[2])
        end
    end
    return string.format("%.4g", v)
end


local function validate(world)
    return #world.domain > 0
end


local function compute_defaults(world)
    -- smallest dx across domain axes
    local dx_min = math.huge
    for _, ax in ipairs(world.domain) do
        local dx = (ax.max - ax.min) / ax.points
        if dx < dx_min then dx_min = dx end
    end

    -- lightest particle mass
    local mass = C.m_electron
    for _, p in ipairs(world.particles) do
        if p.mass < mass then mass = p.mass end
    end

    -- max stable dt from kinetic term: hbar * k_max^2 / (2m) * dt < pi
    local k_max = math.pi / dx_min
    local dt_kinetic = math.pi / (hbar * k_max * k_max / (2 * mass))

    -- max stable dt from potential term: V_max * dt / (2*hbar) < pi
    local v_max = 0
    for _, pot in ipairs(world.potentials) do
        local h = pot.height or 0
        if h > v_max then v_max = h end
        local d = pot.depth or 0
        if d > v_max then v_max = d end
        if pot.type == "harmonic" and pot.k then
            local r2_max = 0
            for i, ax in ipairs(world.domain) do
                local c = (pot.center and pot.center[i]) or 0
                local d1 = math.abs(ax.max - c)
                local d2 = math.abs(ax.min - c)
                local dmax = math.max(d1, d2)
                r2_max = r2_max + dmax * dmax
            end
            local vh = 0.5 * pot.k * r2_max
            if vh > v_max then v_max = vh end
        end
    end
    for _, inter in ipairs(world.interactions) do
        local s = inter.strength or 0
        if s > v_max then v_max = s end
    end

    -- scan custom potential on a coarse grid
    if world.custom_potential then
        local axes = world.domain
        local ndims = #axes
        local sample = 64
        local coords = {}
        local pos = {}
        for d = 1, ndims do coords[d] = 0 end

        local total = 1
        for d = 1, ndims do total = total * sample end

        for _ = 1, total do
            for d = 1, ndims do
                pos[d] = axes[d].min + coords[d] * (axes[d].max - axes[d].min) / sample
            end
            local vv = math.abs(world.custom_potential(table.unpack(pos, 1, ndims)))
            if vv > v_max then v_max = vv end
            for d = ndims, 1, -1 do
                coords[d] = coords[d] + 1
                if coords[d] < sample then break end
                coords[d] = 0
            end
        end
    end

    local dt_potential = (v_max > 0) and (math.pi * 2 * hbar / v_max) or math.huge

    -- wavefunction resolvability warnings
    local ax_offset = 1
    for i, p in ipairs(world.particles) do
        local ndims = #p.position
        for d = 1, ndims do
            local ax = world.domain[ax_offset + d - 1]
            if not ax then break end
            local dx = (ax.max - ax.min) / ax.points
            local mom = p.momentum[d] or 0
            local phase_per_cell = math.abs(mom) * dx / hbar
            if phase_per_cell > math.pi then
                lwrn(string.format("particle %d axis %d: momentum too high for grid", i, d))
                lwrn(string.format("  phase/cell = %.1f rad (max pi), need %d points or lower momentum",
                    phase_per_cell, math.ceil(ax.points * phase_per_cell / math.pi)))
            end
            local w = p.width[d] or p.width[1]
            if w < 2 * dx then
                lwrn(string.format("particle %d axis %d: width %.2e m < 2*dx = %.2e m, poorly resolved",
                    i, d, w, 2 * dx))
            end
        end
        ax_offset = ax_offset + ndims
    end

    -- 10% safety margin on the tighter limit
    local dt = math.min(dt_kinetic, dt_potential) * 0.1

    -- timescale: packet crosses ~20% of domain in 5 wall-seconds
    local L = world.domain[1].max - world.domain[1].min
    local v = 0
    for _, p in ipairs(world.particles) do
        for _, mom in ipairs(p.momentum) do
            v = v + math.abs(mom) / p.mass
        end
    end
    if v == 0 then
        for _, p in ipairs(world.particles) do
            local w = p.width[1] or (L / 10)
            v = v + hbar / (p.mass * w)
        end
    end
    if v < 1e-30 then v = 1e-30 end
    local timescale = (0.2 * L / v) / 5.0

    return dt, timescale
end


local function dump(world)
    linf(string.format("dimensions: %d", world.spatial_dims))
    linf("domain:")
    for i, ax in ipairs(world.domain) do
        linf(string.format("  axis %d: [%sm .. %sm] %d points  dx=%sm",
            i, humanize(ax.min), humanize(ax.max), ax.points,
            humanize((ax.max - ax.min) / ax.points)))
    end
    if #world.particles > 0 then
        linf("particles:")
        for i, p in ipairs(world.particles) do
            local pos = {}
            for _, v in ipairs(p.position) do pos[#pos+1] = humanize(v) .. "m" end
            local mom = {}
            for _, v in ipairs(p.momentum) do mom[#mom+1] = humanize(v) .. " kg·m/s" end
            local wid = {}
            for _, v in ipairs(p.width) do wid[#wid+1] = humanize(v) .. "m" end
            linf(string.format("  %d: mass=%skg  pos={%s}  mom={%s}  w={%s}",
                i, humanize(p.mass), table.concat(pos, ", "),
                table.concat(mom, ", "), table.concat(wid, ", ")))
        end
    end
    if #world.potentials > 0 then
        linf("potentials:")
        for i, pot in ipairs(world.potentials) do
            if pot.type == "barrier" then
                local f, t = {}, {}
                for _, v in ipairs(pot.from) do f[#f+1] = humanize(v) .. "m" end
                for _, v in ipairs(pot.to) do t[#t+1] = humanize(v) .. "m" end
                linf(string.format("  %d: barrier  from={%s}  to={%s}  height=%seV",
                    i, table.concat(f, ", "), table.concat(t, ", "),
                    humanize(pot.height / C.eV)))
            elseif pot.type == "well" then
                local f, t = {}, {}
                for _, v in ipairs(pot.from) do f[#f+1] = humanize(v) .. "m" end
                for _, v in ipairs(pot.to) do t[#t+1] = humanize(v) .. "m" end
                linf(string.format("  %d: well  from={%s}  to={%s}  depth=%seV",
                    i, table.concat(f, ", "), table.concat(t, ", "),
                    humanize(pot.depth / C.eV)))
            elseif pot.type == "harmonic" then
                local c = {}
                for _, v in ipairs(pot.center) do c[#c+1] = humanize(v) .. "m" end
                linf(string.format("  %d: harmonic  center={%s}  k=%s N/m",
                    i, table.concat(c, ", "), humanize(pot.k)))
            else
                linf(string.format("  %d: %s", i, pot.type))
            end
        end
    end
    if #world.interactions > 0 then
        linf("interactions:")
        for i, inter in ipairs(world.interactions) do
            linf(string.format("  %d: %s  particles={%d,%d}  softening=%sm",
                i, inter.type, inter.particles[1], inter.particles[2],
                humanize(inter.softening)))
        end
    end
    if #world.simulations > 0 then
        linf("simulations:")
        for i, sim in ipairs(world.simulations) do
            local res = sim.resolution and tostring(sim.resolution) or "domain"
            linf(string.format("  %d: \"%s\"  mode=%s  resolution=%s  dt=%ss",
                i, sim.name, sim.mode, res, humanize(sim.dt)))
        end
    end
    if world.absorbing_boundary then
        linf(string.format("absorbing boundary: width=%.3f  strength=%s",
            world.absorb_width,
            world.absorb_strength > 0 and humanize(world.absorb_strength) or "auto"))
    end
end

return {
    validate = validate,
    compute_defaults = compute_defaults,
    dump = dump,
}

end -- build

return build
