-- Quantum simulator prelude
-- This is the entry point. C++ runs this file.
-- The user script filename is passed in the global `script`.

-- accumulated world state
local world = {
    spatial_dims = nil,
    domain = {},
    particles = {},
    potentials = {},
    interactions = {},
    simulations = {},
}

-- build the user script environment
local env = {}

-- lua stdlib
env.math   = math
env.string = string
env.table  = table
env.print  = print
env.pairs  = pairs
env.ipairs = ipairs
env.type   = type
env.error  = error
env.tostring = tostring
env.tonumber = tonumber
env.assert = assert
env.pcall  = pcall
env.unpack = table.unpack
env.require = nil  -- no module loading from user scripts

-- units
env.nm = 1e-9
env.um = 1e-6
env.mm = 1e-3
env.m  = 1.0
env.fs = 1e-15
env.ps = 1e-12
env.ns = 1e-9

-- physical constants
local hbar       = 1.054571817e-34      -- J·s
local e_charge   = 1.602176634e-19      -- C
local eV         = 1.602176634e-19      -- J per eV
local m_electron = 9.1093837015e-31     -- kg
local m_proton   = 1.67262192369e-27    -- kg

local k_coulomb  = 8.9875517873681764e9   -- N·m²/C²

env.hbar       = hbar
env.e_charge   = e_charge
env.eV         = eV
env.m_electron = m_electron
env.m_proton   = m_proton
env.k_coulomb  = k_coulomb

-- API functions

function env.dimensions(n)
    world.spatial_dims = n
end

function env.domain(axes)
    world.domain = axes
end

function env.def_particle(spec)
    return {
        mass = spec.mass or error("def_particle: mass required"),
        charge = spec.charge or 0,
    }
end

function env.particle(species, spec)
    world.particles[#world.particles + 1] = {
        mass     = species.mass,
        charge   = species.charge,
        position = spec.position or error("particle: position required"),
        momentum = spec.momentum or error("particle: momentum required"),
        width    = spec.width or error("particle: width required"),
    }
end

function env.barrier(spec)
    world.potentials[#world.potentials + 1] = {
        type   = "barrier",
        from   = spec.from or error("barrier: from required"),
        to     = spec.to or error("barrier: to required"),
        height = spec.height or error("barrier: height required"),
    }
end

function env.well(spec)
    world.potentials[#world.potentials + 1] = {
        type   = "well",
        from   = spec.from or error("well: from required"),
        to     = spec.to or error("well: to required"),
        depth  = spec.depth or error("well: depth required"),
    }
end

function env.harmonic(spec)
    world.potentials[#world.potentials + 1] = {
        type   = "harmonic",
        center = spec.center or error("harmonic: center required"),
        k      = spec.k or error("harmonic: k (spring constant) required"),
    }
end

function env.interaction(spec)
    world.interactions[#world.interactions + 1] = {
        type       = spec.type or "coulomb",
        particles  = spec.particles or error("interaction: particles required"),
        softening  = spec.softening or 0,
        strength   = spec.strength or 0,
        width      = spec.width or 0,
    }
end

function env.simulate(spec)
    world.simulations[#world.simulations + 1] = {
        name       = spec.name or ("sim" .. #world.simulations + 1),
        mode       = spec.mode or "joint",
        resolution = spec.resolution,
        dt         = spec.dt or error("simulate: dt required"),
    }
end

-- load and run user script in sandboxed environment
local chunk, load_err = loadfile(script, "t", env)
if not chunk then
    io.stderr:write(load_err .. "\n")
    return nil
end

local ok, err = xpcall(chunk, function(msg)
    return debug.traceback(msg, 2)
end)

if not ok then
    io.stderr:write(err .. "\n")
    return nil
end

-- humanize a value with SI prefix
local function humanize(v)
    local prefixes = {
        { 1e12, "T" }, { 1e9, "G" }, { 1e6, "M" }, { 1e3, "k" },
        { 1, "" },
        { 1e-3, "m" }, { 1e-6, "μ" }, { 1e-9, "n" },
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

local function dump()
    print(string.format("dimensions: %d", world.spatial_dims))
    print("domain:")
    for i, ax in ipairs(world.domain) do
        print(string.format("  axis %d: [%sm .. %sm] %d points  dx=%sm",
            i, humanize(ax.min), humanize(ax.max), ax.points,
            humanize((ax.max - ax.min) / ax.points)))
    end
    if #world.particles > 0 then
        print("particles:")
        for i, p in ipairs(world.particles) do
            local pos = {}
            for _, v in ipairs(p.position) do pos[#pos+1] = humanize(v) .. "m" end
            local mom = {}
            for _, v in ipairs(p.momentum) do mom[#mom+1] = humanize(v) .. " kg·m/s" end
            print(string.format("  %d: mass=%skg  pos={%s}  mom={%s}  width=%sm",
                i, humanize(p.mass), table.concat(pos, ", "),
                table.concat(mom, ", "), humanize(p.width)))
        end
    end
    if #world.potentials > 0 then
        print("potentials:")
        for i, pot in ipairs(world.potentials) do
            if pot.type == "barrier" then
                local f, t = {}, {}
                for _, v in ipairs(pot.from) do f[#f+1] = humanize(v) .. "m" end
                for _, v in ipairs(pot.to) do t[#t+1] = humanize(v) .. "m" end
                print(string.format("  %d: barrier  from={%s}  to={%s}  height=%seV",
                    i, table.concat(f, ", "), table.concat(t, ", "),
                    humanize(pot.height / eV)))
            elseif pot.type == "well" then
                local f, t = {}, {}
                for _, v in ipairs(pot.from) do f[#f+1] = humanize(v) .. "m" end
                for _, v in ipairs(pot.to) do t[#t+1] = humanize(v) .. "m" end
                print(string.format("  %d: well  from={%s}  to={%s}  depth=%seV",
                    i, table.concat(f, ", "), table.concat(t, ", "),
                    humanize(pot.depth / eV)))
            elseif pot.type == "harmonic" then
                local c = {}
                for _, v in ipairs(pot.center) do c[#c+1] = humanize(v) .. "m" end
                print(string.format("  %d: harmonic  center={%s}  k=%s N/m",
                    i, table.concat(c, ", "), humanize(pot.k)))
            else
                print(string.format("  %d: %s", i, pot.type))
            end
        end
    end
    if #world.interactions > 0 then
        print("interactions:")
        for i, inter in ipairs(world.interactions) do
            print(string.format("  %d: %s  particles={%d,%d}  softening=%sm",
                i, inter.type, inter.particles[1], inter.particles[2],
                humanize(inter.softening)))
        end
    end
    if #world.simulations > 0 then
        print("simulations:")
        for i, sim in ipairs(world.simulations) do
            local res = sim.resolution and tostring(sim.resolution) or "domain"
            print(string.format("  %d: \"%s\"  mode=%s  resolution=%s  dt=%ss",
                i, sim.name, sim.mode, res, humanize(sim.dt)))
        end
    end
end

-- validation
if not world.spatial_dims then
    io.stderr:write("error: dimensions() not called\n")
    return nil
end

if #world.domain == 0 then
    io.stderr:write("error: domain() not called\n")
    return nil
end

if #world.domain ~= world.spatial_dims then
    io.stderr:write("error: domain has " .. #world.domain
        .. " axes but dimensions() said " .. world.spatial_dims .. "\n")
    return nil
end

-- compute sensible defaults from experiment parameters
local function compute_defaults()
    -- find smallest dx across domain axes
    local dx_min = math.huge
    for _, ax in ipairs(world.domain) do
        local dx = (ax.max - ax.min) / ax.points
        if dx < dx_min then dx_min = dx end
    end

    -- find lightest particle mass
    local mass = m_electron  -- fallback
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
        -- harmonic: V = 0.5 * k * r_max^2, where r_max is the domain edge
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
    -- also check interaction strengths
    for _, inter in ipairs(world.interactions) do
        local s = inter.strength or 0
        if s > v_max then v_max = s end
    end
    local dt_potential = (v_max > 0) and (math.pi * 2 * hbar / v_max) or math.huge

    -- take the smaller limit, then 10% safety margin
    local dt_max = math.min(dt_kinetic, dt_potential)
    local dt = dt_max * 0.1

    -- timescale: how long for the packet to cross ~20% of the domain?
    -- pick a reasonable "watch time" of ~5 seconds
    local L = world.domain[1].max - world.domain[1].min
    local v = 0
    for _, p in ipairs(world.particles) do
        for _, mom in ipairs(p.momentum) do
            v = v + math.abs(mom) / p.mass
        end
    end
    if v < 1 then v = 1 end
    local crossing_time = 0.2 * L / v  -- sim time to cross 20% of domain
    local timescale = crossing_time / 5.0  -- do it in 5 wall seconds

    return dt, timescale
end

local default_dt, default_timescale = compute_defaults()

-- default simulation if none specified
if #world.simulations == 0 then
    world.simulations[1] = {
        name = "default",
        mode = "joint",
        resolution = nil,
        dt = default_dt,
    }
end

-- set timescale on world for C++ to pick up
world.timescale = default_timescale

dump()

return world
