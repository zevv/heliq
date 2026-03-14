# Lua Experiment API

## Instructions

**IMMUTABLE. This section MUST be preserved verbatim in every design
document, every version. Do NOT edit, summarize, reword, or omit.**

Living document. Born at conception, lives with the project. Early versions
will be wrong. That is expected. The document converges on truth over time.

### Sections

- **Rationale** — the problem, stakeholders, forces. No solutions.
- **FR-NNN** — what the system must do. Amend in place, mark `(revised)`.
- **TBD-NNN** — open questions = preserved decision space. May linger.
- **DEC-NNN** — decisions made. TBD-N becomes DEC-N (same number).
- **ACT-NNN** — implementation tasks. Move to Done when finished.
- **Done** — one-liner per completed ACT.
- **Scratch** — raw dump. User or agent, anytime. Process periodically:
  promote to FR/TBD/ACT or discard.

### Process

The user starts with prose, ramblings, half-formed ideas. That is the input.
The agent's job is to extract structure from this — FRs, TBDs, DECs — by
**interviewing the user. One question at a time.** Do not batch questions.
Do not assume answers. The interview continues throughout the life of the
document, not just at creation.

### Rules

1. **NEVER renumber.** Numbers are identifiers, not order. Renumbering
   breaks every cross-reference. Do not do it.
2. **BEFORE EVERY DEC: scan the entire TBD list.** Does this decision
   narrow or foreclose ANY open TBD? If yes, you MUST state it in the DEC.
   This is not optional. This is the most important rule in this document.
   Unknown unknowns exist — this will not catch everything. Do it anyway.
3. **When an FR changes: scan ALL downstream DECs, TBDs, ACTs for impact.**
   Do NOT proceed until impacts are assessed and flagged.
4. No 1:1 mapping between sections. Cross-reference freely.
5. **TBDs linger** until they block the critical path. Do NOT resolve
   TBDs early to tidy the document.
6. History is in git. Do NOT version items inside the document.
7. **Build for now.** Not for hypothetical futures. Requirements WILL
   change.

---

## Rationale

The Lua script is the primary user interface for defining experiments.
It must feel natural, readable, and self-documenting. The user is not a
programmer — they are a physicist (or aspiring one) describing an
experiment. The script should read like a lab notebook, not like C++ config
serialization.

The script builds up a world description through function calls that
accumulate state. At the end, the accumulated state is returned as the
experiment definition for C++ ingestion.

## Functional Requirements

- FR-001: The user describes an experiment through a sequence of function
  calls that build up a world. Not by constructing a raw table. The API
  provides the vocabulary; the user speaks in it.

- FR-002: Physical constants and unit helpers are available globally.
  The user writes `5 * eV`, `100 * nm`, `m_electron`, not raw numbers.

- FR-003: Particle species are defined once, then placed. Define an
  electron with its mass and charge, then place instances of it with
  position, momentum, and width. Species are reusable — define once,
  place many.

- FR-004: Potentials are added to the world as geometric/field primitives.
  Barriers, wells, slits, etc. Each with physical parameters in SI units.

- FR-005: The script must be runnable standalone (`lua script.lua`) for
  syntax checking, even if the world functions are stubs. Errors in the
  script should produce clear messages with line numbers.

## To Be Decided

- TBD-001: ~~Global world object vs module functions~~ Resolved → DEC-001.

- TBD-002: ~~Slits~~ Build from barriers for now. Convenience functions
  (double_slit, etc.) can be added as Lua helpers later.

- TBD-003: ~~What does the script return?~~ Resolved → DEC-002.

## Decisions

- DEC-001: Plain global functions, no `world.` prefix. `dimensions(1)`,
  `barrier{...}`, `particle(...)`. This is a single-purpose scripting
  environment, not a general Lua program. No namespace collision risk.
  Resolves TBD-001.

- DEC-002: Three-phase execution. (1) Our Lua prelude runs: defines global
  functions, unit constants, internal state accumulator. (2) User script
  runs: calls those functions. No explicit return needed. (3) Our Lua
  postlude runs: validates, transforms, returns the accumulated state as
  a table. C++ walks that final table. User never sees the prelude/postlude.
  Resolves TBD-003.

## Actionable Items

- ACT-001: Draft the API — function signatures, naming, calling convention.
- ACT-002: Write the first experiment script using the API.
- ACT-003: Implement the Lua-side library (pure Lua, accumulates state).
- ACT-004: Implement the C++ loader that reads the accumulated state.

## Done

## Scratch

Agent note: this section is a dumping ground for raw thoughts, ideas, and
half-formed requirements. The agent will periodically review, consolidate
into FR/TBD/ACT entries, and confer with the user when needed.

Rough API sketch from discussion:

```lua
-- units and constants available globally
-- nm, um, mm, eV, m_electron, e_charge, hbar, etc.

dimensions(1)

domain {
    { min = -5 * um, max = 5 * um, points = 1024 },
}

-- define a particle species
electron = def_particle {
    mass = m_electron,
    charge = -e_charge,
}

-- place a particle instance
particle(electron, {
    position = { -2 * um },
    momentum = { 1e-24 },
    width = 0.5 * um,
})

-- add potentials
barrier {
    from = { -50 * nm },
    to = { 50 * nm },
    height = 5 * eV,
}

-- future:
-- gravity()
-- electric_field { direction = {1, 0}, strength = 1e6 }
-- absorbing_boundary { width = 0.15 }
```
