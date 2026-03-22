-- Experiment API
-- Functions exposed to user scripts via the sandbox environment.
-- All functions accumulate into the `world` table passed in.
-- After the script runs, the prelude calls build_potential() and
-- build_psi_init() to compose the accumulated primitives into
-- the final functions that C++ will sample.

local function build(world, C)
    local api = {}
    local hbar = C.hbar
    local k_coulomb = C.k_coulomb

    function api.description(title, text)
        world.title = title
        world.description = text or ""
    end

    function api.domain(axes)
        world.domain = axes
        world.spatial_dims = #axes
    end

    function api.def_particle(spec)
        return {
            mass = spec.mass or error("def_particle: mass required"),
            charge = spec.charge or 0,
        }
    end

    function api.particle(species, spec)
        local pos = spec.position or error("particle: position required")
        local w = spec.width or error("particle: width required")
        local width
        if type(w) == "number" then
            width = {}
            for i = 1, #pos do width[i] = w end
        elseif type(w) == "table" then
            width = w
            if #width ~= #pos then
                error("particle: width table must have same length as position")
            end
        else
            error("particle: width must be a number or table")
        end
        world.particles[#world.particles + 1] = {
            mass     = species.mass,
            charge   = species.charge,
            position = pos,
            momentum = spec.momentum or error("particle: momentum required"),
            width    = width,
        }
    end

    function api.barrier(spec)
        world.potentials[#world.potentials + 1] = {
            type   = "barrier",
            from   = spec.from or error("barrier: from required"),
            to     = spec.to or error("barrier: to required"),
            height = spec.height or error("barrier: height required"),
        }
    end

    function api.well(spec)
        world.potentials[#world.potentials + 1] = {
            type   = "well",
            from   = spec.from or error("well: from required"),
            to     = spec.to or error("well: to required"),
            depth  = spec.depth or error("well: depth required"),
        }
    end

    function api.harmonic(spec)
        world.potentials[#world.potentials + 1] = {
            type   = "harmonic",
            center = spec.center or error("harmonic: center required"),
            k      = spec.k or error("harmonic: k (spring constant) required"),
        }
    end

    function api.interaction(spec)
        world.interactions[#world.interactions + 1] = {
            type       = spec.type or "coulomb",
            particles  = spec.particles or error("interaction: particles required"),
            softening  = spec.softening or 0,
            strength   = spec.strength or 0,
            width      = spec.width or 0,
            power      = spec.power or 1,
        }
    end

    function api.simulate(spec)
        world.simulations[#world.simulations + 1] = {
            name       = spec.name or ("sim" .. #world.simulations + 1),
            mode       = spec.mode or "joint",
            resolution = spec.resolution,
            dt         = spec.dt or error("simulate: dt required"),
        }
    end

    function api.potential(fn)
        world.custom_potential = fn
    end

    function api.psi_init(fn)
        world.psi_init = fn
    end

    function api.absorbing_boundary(spec)
        world.absorbing_boundary = true
        world.absorb_width = spec.width or 0.02
        world.absorb_strength = spec.strength or 0
    end

    -- compose accumulated potentials + interactions + custom_potential
    -- into a single world.potential function: (pos...) → V
    function api.build_potential()
        local ndims = world.spatial_dims
        local np = #world.particles
        local sd = world.dims_per_particle

        -- build individual potential closures
        local fns = {}

        for _, pot in ipairs(world.potentials) do
            if pot.type == "barrier" then
                local from, to, h = pot.from, pot.to, pot.height
                fns[#fns+1] = function(pos)
                    for d = 1, ndims do
                        if pos[d] < from[d] or pos[d] > to[d] then return 0 end
                    end
                    return h
                end
            elseif pot.type == "well" then
                local from, to, depth = pot.from, pot.to, pot.depth
                fns[#fns+1] = function(pos)
                    for d = 1, ndims do
                        if pos[d] < from[d] or pos[d] > to[d] then return 0 end
                    end
                    return -depth
                end
            elseif pot.type == "harmonic" then
                local center, k = pot.center, pot.k
                fns[#fns+1] = function(pos)
                    local r2 = 0
                    for d = 1, ndims do
                        local dx = pos[d] - center[d]
                        r2 = r2 + dx * dx
                    end
                    return 0.5 * k * r2
                end
            end
        end

        -- interactions (config-space distance between particle axis groups)
        for _, inter in ipairs(world.interactions) do
            local pa = inter.particles[1] - 1  -- 0-based
            local pb = inter.particles[2] - 1
            if inter.type == "coulomb" then
                local soft = inter.softening
                local strength = inter.strength
                local power = inter.power
                -- find charges from particles
                local q_a = (world.particles[pa+1] or {}).charge or 0
                local q_b = (world.particles[pb+1] or {}).charge or 0
                fns[#fns+1] = function(pos)
                    local d2 = 0
                    for d = 1, sd do
                        local dx = pos[pa * sd + d] - pos[pb * sd + d]
                        d2 = d2 + dx * dx
                    end
                    local r2s = d2 + soft * soft
                    local denom = r2s ^ (power * 0.5)
                    if power == 1 then
                        return strength * k_coulomb * q_a * q_b / denom
                    else
                        local s_p = (soft * soft) ^ (power * 0.5)
                        return strength * s_p / denom
                    end
                end
            elseif inter.type == "contact" then
                local strength = inter.strength
                local w2 = inter.width * inter.width
                fns[#fns+1] = function(pos)
                    local d2 = 0
                    for d = 1, sd do
                        local dx = pos[pa * sd + d] - pos[pb * sd + d]
                        d2 = d2 + dx * dx
                    end
                    return strength * math.exp(-d2 / w2)
                end
            end
        end

        local custom = world.custom_potential

        if #fns == 0 and not custom then
            world.potential = nil
            return
        end

        world.potential = function(...)
            local pos = {...}
            local v = 0
            for _, fn in ipairs(fns) do
                v = v + fn(pos)
            end
            if custom then
                v = v + custom(...)
            end
            return v
        end
    end

    -- compose accumulated particles into a Gaussian product psi_init
    -- only if user didn't provide an explicit psi_init
    function api.build_psi_init()
        if world.psi_init then return end
        if #world.particles == 0 then return end

        local ndims = world.spatial_dims
        local np = #world.particles
        local sd = world.dims_per_particle

        local parts = {}
        for i, p in ipairs(world.particles) do
            parts[i] = {
                pos = p.position,
                mom = p.momentum,
                width = p.width,
                ax0 = (i - 1) * sd,
                nd = sd,
            }
        end

        world.psi_init = function(...)
            local pos = {...}
            local re, im = 1, 0
            for _, p in ipairs(parts) do
                local envelope = 0
                local phase = 0
                for d = 1, p.nd do
                    local ax = p.ax0 + d
                    local dx = pos[ax] - p.pos[d]
                    local w = p.width[d]
                    envelope = envelope + dx * dx / (4 * w * w)
                    phase = phase + p.mom[d] * pos[ax] / hbar
                end
                local amp = math.exp(-envelope)
                local c, s = math.cos(phase), math.sin(phase)
                local nr = re * amp * c - im * amp * s
                local ni = re * amp * s + im * amp * c
                re, im = nr, ni
            end
            return re, im
        end
    end

    -- derive mass[] from particles and assign to domain axes
    function api.build_mass()
        local ndims = world.spatial_dims
        local np = #world.particles
        local sd = world.dims_per_particle

        world.mass = {}
        for i = 1, ndims do
            world.mass[i] = C.m_electron  -- fallback
        end
        for i, p in ipairs(world.particles) do
            for d = 1, sd do
                local ax = (i - 1) * sd + d
                if ax <= ndims then
                    world.mass[ax] = p.mass
                end
            end
        end
    end

    return api
end

return build
