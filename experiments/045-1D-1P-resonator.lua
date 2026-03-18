description("Fabry-Pérot Resonator", [[
A single barrier acts as a quantum Fabry-Pérot cavity. The wavepacket
partially enters the barrier and bounces between its two edges,
leaking out both sides on each round trip.

This is the quantum analog of light bouncing inside a thin glass
plate, or a ringing bell. The trapped amplitude is a quasi-bound
state that decays exponentially — each emitted pulse is weaker
than the last.

The barrier height equals the kinetic energy (1×), so transmission
is partial. Watch the trace widget (F4) to see the round-trip
bounces as diagonal stripes inside the barrier region.

Try: increase height to 2× for slower decay (more bounces),
decrease to 0.5× for fast transmission (fewer bounces).
Widen the barrier to see more round trips before decay.]])

domain {
    { min = -200 * nm, max = 200 * nm, points = 1024 },
}

electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

local energy = 0.1 * eV
local momentum = math.sqrt(2 * m_electron * energy)

particle(electron, {
    position = { -30 * nm },
    momentum = { momentum },
    width = 5 * nm,
})

-- thin barrier, slightly above kinetic energy
barrier {
    from = { -7 * nm },
    to   = {  7 * nm },
    height = 0.8 * energy,
}
