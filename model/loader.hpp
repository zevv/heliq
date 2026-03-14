#pragma once

#include "setup.hpp"

// Load experiment setup from a Lua script.
// Returns true on success, fills setup.
// On failure, returns false and prints error to stderr.

bool load_setup(const char *filename, Setup &setup);
