
#include <string.h>

#include <lua5.4/lua.hpp>

#include "loader.hpp"
#include "log.hpp"


static double getfield_number(lua_State *L, int idx, const char *key, double def = 0.0)
{
	lua_getfield(L, idx, key);
	double v = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : def;
	lua_pop(L, 1);
	return v;
}


static const char *getfield_string(lua_State *L, int idx, const char *key)
{
	lua_getfield(L, idx, key);
	const char *v = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
	lua_pop(L, 1);
	return v;
}


static bool load_domain(lua_State *L, Setup &setup)
{
	lua_getfield(L, -1, "domain");
	if(!lua_istable(L, -1)) {
		lerr("missing 'domain' table");
		lua_pop(L, 1);
		return false;
	}

	int naxes = (int)lua_rawlen(L, -1);
	if(naxes > MAX_RANK) naxes = MAX_RANK;

	for(int i = 0; i < naxes; i++) {
		lua_rawgeti(L, -1, i + 1);
		if(lua_istable(L, -1)) {
			setup.domain[i].min = getfield_number(L, -1, "min");
			setup.domain[i].max = getfield_number(L, -1, "max");
			setup.domain[i].points = (int)getfield_number(L, -1, "points");
			setup.domain[i].spatial = true;
		}
		lua_pop(L, 1);
	}

	lua_pop(L, 1);
	return true;
}


static bool load_mass(lua_State *L, Setup &setup)
{
	lua_getfield(L, -1, "mass");
	if(!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return true;
	}

	int n = (int)lua_rawlen(L, -1);
	if(n > MAX_RANK) n = MAX_RANK;
	for(int i = 0; i < n; i++) {
		lua_rawgeti(L, -1, i + 1);
		setup.mass[i] = lua_tonumber(L, -1);
		lua_pop(L, 1);
	}

	lua_pop(L, 1);
	return true;
}


// sample a Lua function on the grid, storing results as complex values.
// fn(x,y,...) → re [, im]
static bool sample_function(lua_State *L, const char *field, Setup &setup,
                            std::vector<psi_t> &out)
{
	lua_getfield(L, -1, field);
	if(!lua_isfunction(L, -1)) {
		lua_pop(L, 1);
		return true;
	}

	int func_idx = lua_gettop(L);

	int rank = 0;
	for(int i = 0; i < MAX_RANK; i++) {
		if(setup.domain[i].points <= 0) break;
		rank++;
	}

	Grid grid;
	grid.rank = rank;
	for(int i = 0; i < rank; i++)
		grid.axes[i] = setup.domain[i];
	grid.compute_strides();

	out.resize(grid.total_points());

	grid.each([&](size_t idx, const int *, const double *pos) {
		lua_pushvalue(L, func_idx);
		for(int d = 0; d < rank; d++)
			lua_pushnumber(L, pos[d]);
		lua_call(L, rank, 2);
		double re = lua_tonumber(L, -2);
		double im = lua_tonumber(L, -1);  // nil → 0
		out[idx] = psi_t((float)re, (float)im);
		lua_pop(L, 2);
	});

	lua_pop(L, 1);
	return true;
}


static bool load_simulations(lua_State *L, Setup &setup)
{
	lua_getfield(L, -1, "simulations");
	if(!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return true;
	}

	int n = (int)lua_rawlen(L, -1);
	for(int i = 0; i < n; i++) {
		lua_rawgeti(L, -1, i + 1);
		if(lua_istable(L, -1)) {
			SimConfig sim{};

			const char *name = getfield_string(L, -1, "name");
			if(name) sim.name = name;

			const char *mode = getfield_string(L, -1, "mode");
			if(mode && strcmp(mode, "factored") == 0)
				sim.mode = SimMode::Factored;

			sim.resolution = (int)getfield_number(L, -1, "resolution");
			sim.dt = getfield_number(L, -1, "dt");

			setup.simulations.push_back(sim);
		}
		lua_pop(L, 1);
	}

	lua_pop(L, 1);
	return true;
}


static int lua_log(lua_State *L, Log::Level level)
{
	int n = lua_gettop(L);
	luaL_Buffer b;
	luaL_buffinit(L, &b);
	for(int i = 1; i <= n; i++) {
		if(i > 1) luaL_addchar(&b, ' ');
		luaL_addstring(&b, luaL_tolstring(L, i, nullptr));
		lua_pop(L, 1);
	}
	luaL_pushresult(&b);
	log_write(level, "lua", "%s", lua_tostring(L, -1));
	return 0;
}

static int lua_linf(lua_State *L) { return lua_log(L, Log::Inf); }
static int lua_lwrn(lua_State *L) { return lua_log(L, Log::Wrn); }
static int lua_lerr(lua_State *L) { return lua_log(L, Log::Err); }


bool load_setup(const char *script, Setup &setup, bool verbose)
{
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	// register log functions
	lua_pushcfunction(L, lua_linf);
	lua_setglobal(L, "linf");
	lua_pushcfunction(L, lua_lwrn);
	lua_setglobal(L, "lwrn");
	lua_pushcfunction(L, lua_lerr);
	lua_setglobal(L, "lerr");

	lua_pushstring(L, script);
	lua_setglobal(L, "script");

	lua_pushboolean(L, verbose);
	lua_setglobal(L, "verbose");

	if(luaL_dofile(L, "lua/prelude.lua") != LUA_OK) {
		lerr("%s", lua_tostring(L, -1));
		lua_close(L);
		return false;
	}

	if(!lua_istable(L, -1)) {
		lerr("prelude did not return a table");
		lua_close(L);
		return false;
	}

	// metadata
	lua_getfield(L, -1, "title");
	if(lua_isstring(L, -1)) setup.title = lua_tostring(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, -1, "description");
	if(lua_isstring(L, -1)) setup.description = lua_tostring(L, -1);
	lua_pop(L, 1);

	setup.spatial_dims = (int)getfield_number(L, -1, "spatial_dims");
	setup.n_particles = (int)getfield_number(L, -1, "n_particles");
	setup.timescale = getfield_number(L, -1, "timescale", 1e-15);
	setup.default_timescale = setup.timescale;

	// absorbing boundary
	lua_getfield(L, -1, "absorbing_boundary");
	setup.absorbing_boundary = lua_toboolean(L, -1);
	lua_pop(L, 1);
	if(setup.absorbing_boundary) {
		setup.absorb_width = getfield_number(L, -1, "absorb_width", 0.02);
		setup.absorb_strength = getfield_number(L, -1, "absorb_strength", 0.0);
	}

	bool ok = load_domain(L, setup)
	       && load_mass(L, setup)
	       && sample_function(L, "potential", setup, setup.potential)
	       && sample_function(L, "psi_init", setup, setup.psi_init)
	       && load_simulations(L, setup);

	if(!setup.simulations.empty())
		setup.default_dt = fabs(setup.simulations[0].dt);

	lua_close(L);
	return ok;
}
