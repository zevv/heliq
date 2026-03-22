-- Prelude: orchestrator between user scripts and the simulator.
-- C++ runs this file via luaL_dofile. Returns the world table.
-- Globals set by C++: `script` (user script path), `verbose` (bool).

local C = dofile("lua/constants.lua")
local build_api = dofile("lua/api.lua")
local build_defaults = dofile("lua/defaults.lua")

local defaults = build_defaults(C)

-- world state, populated by the user script via api functions
local world = {
    title = "",
    description = "",
    spatial_dims = nil,
    domain = {},
    particles = {},
    potentials = {},
    interactions = {},
    simulations = {},
    absorbing_boundary = false,
    absorb_width = 0.02,
    absorb_strength = 0,
}

local api = build_api(world, C)

-- sandbox: lua stdlib + constants + user-facing api
local env = {}

env.math   = math
env.string = string
env.table  = table
env.print  = linf
env.pairs  = pairs
env.ipairs = ipairs
env.type   = type
env.error  = error
env.tostring = tostring
env.tonumber = tonumber
env.assert = assert
env.pcall  = pcall
env.unpack = table.unpack

for k, v in pairs(C) do env[k] = v end

-- expose only user-facing api (not build_ functions)
env.description        = api.description
env.domain             = api.domain
env.def_particle       = api.def_particle
env.particle           = api.particle
env.barrier            = api.barrier
env.well               = api.well
env.harmonic           = api.harmonic
env.interaction        = api.interaction
env.simulate           = api.simulate
env.potential          = api.potential
env.psi_init           = api.psi_init
env.absorbing_boundary = api.absorbing_boundary

-- load and run user script
local chunk, load_err = loadfile(script, "t", env)
if not chunk then error(load_err, 0) end

local ok, err = xpcall(chunk, function(msg)
    return debug.traceback(msg, 2)
end)
if not ok then error(err, 0) end

-- post-processing
if not defaults.validate(world) then
    error("domain() not called", 0)
end

-- compose primitives into final functions
api.build_potential()
api.build_psi_init()
api.build_mass()

local dt, timescale = defaults.compute_defaults(world)

if #world.simulations == 0 then
    world.simulations[1] = {
        name = "default",
        mode = "joint",
        resolution = nil,
        dt = dt,
    }
end

world.timescale = timescale

-- config space info for C++
local np = #world.particles
world.n_particles = np

defaults.dump(world)

return world
