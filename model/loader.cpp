
#include <stdio.h>
#include <string.h>

#include <lua5.4/lua.hpp>

#include "loader.hpp"


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


static int read_number_array(lua_State *L, int idx, double *out, int max)
{
	int n = 0;
	int len = (int)lua_rawlen(L, idx);
	if(len > max) len = max;
	for(int i = 1; i <= len; i++) {
		lua_rawgeti(L, idx, i);
		if(lua_isnumber(L, -1))
			out[n++] = lua_tonumber(L, -1);
		lua_pop(L, 1);
	}
	return n;
}


static void read_array_field(lua_State *L, int idx, const char *key, double *out, int max)
{
	lua_getfield(L, idx, key);
	if(lua_istable(L, -1))
		read_number_array(L, -1, out, max);
	lua_pop(L, 1);
}


static bool load_domain(lua_State *L, Setup &setup)
{
	lua_getfield(L, -1, "domain");
	if(!lua_istable(L, -1)) {
		fprintf(stderr, "loader: missing 'domain' table\n");
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


static bool load_particles(lua_State *L, Setup &setup)
{
	lua_getfield(L, -1, "particles");
	if(!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return true;
	}

	int n = (int)lua_rawlen(L, -1);
	for(int i = 0; i < n; i++) {
		lua_rawgeti(L, -1, i + 1);
		if(lua_istable(L, -1)) {
			Particle p{};
			p.mass   = getfield_number(L, -1, "mass");
			p.charge = getfield_number(L, -1, "charge");
			p.width  = getfield_number(L, -1, "width");
			read_array_field(L, -1, "position", p.position, MAX_RANK);
			read_array_field(L, -1, "momentum", p.momentum, MAX_RANK);
			setup.particles.push_back(p);
		}
		lua_pop(L, 1);
	}

	lua_pop(L, 1);
	return true;
}


static bool load_potentials(lua_State *L, Setup &setup)
{
	lua_getfield(L, -1, "potentials");
	if(!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return true;
	}

	int n = (int)lua_rawlen(L, -1);
	for(int i = 0; i < n; i++) {
		lua_rawgeti(L, -1, i + 1);
		if(lua_istable(L, -1)) {
			Potential pot{};

			const char *type = getfield_string(L, -1, "type");
			if(type) {
				if(strcmp(type, "barrier") == 0)   pot.type = Potential::Barrier;
				else if(strcmp(type, "well") == 0) pot.type = Potential::Well;
				else if(strcmp(type, "harmonic") == 0) pot.type = Potential::Harmonic;
				else if(strcmp(type, "absorbing") == 0) pot.type = Potential::Absorbing;
				else fprintf(stderr, "loader: unknown potential type '%s'\n", type);
			}

			pot.height = getfield_number(L, -1, "height");
			pot.depth  = getfield_number(L, -1, "depth");
			pot.k      = getfield_number(L, -1, "k");
			read_array_field(L, -1, "from", pot.from, MAX_RANK);
			read_array_field(L, -1, "to", pot.to, MAX_RANK);
			read_array_field(L, -1, "center", pot.center, MAX_RANK);

			setup.potentials.push_back(pot);
		}
		lua_pop(L, 1);
	}

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


bool load_setup(const char *script, Setup &setup, bool verbose)
{
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	// set the user script path for the prelude
	lua_pushstring(L, script);
	lua_setglobal(L, "script");

	lua_pushboolean(L, verbose);
	lua_setglobal(L, "verbose");

	// run prelude (which runs the user script internally)
	if(luaL_dofile(L, "lua/prelude.lua") != LUA_OK) {
		fprintf(stderr, "loader: %s\n", lua_tostring(L, -1));
		lua_close(L);
		return false;
	}

	if(!lua_istable(L, -1)) {
		fprintf(stderr, "loader: prelude did not return a table\n");
		lua_close(L);
		return false;
	}

	setup.spatial_dims = (int)getfield_number(L, -1, "spatial_dims");

	bool ok = load_domain(L, setup)
	       && load_particles(L, setup)
	       && load_potentials(L, setup)
	       && load_simulations(L, setup);

	lua_close(L);
	return ok;
}
