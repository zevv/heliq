-- SI units and physical constants

local C = {}

-- length
C.nm = 1e-9
C.um = 1e-6
C.mm = 1e-3
C.m  = 1.0
C.fm = 1e-15
C.pm = 1e-12

-- time
C.fs = 1e-15
C.ps = 1e-12
C.ns = 1e-9

-- physics
C.hbar       = 1.054571817e-34         -- J·s
C.e_charge   = 1.602176634e-19         -- C
C.eV         = 1.602176634e-19         -- J per eV
C.keV        = 1e3 * C.eV
C.MeV        = 1e6 * C.eV
C.m_electron = 9.1093837015e-31        -- kg
C.m_proton   = 1.67262192369e-27       -- kg
C.k_coulomb  = 8.9875517873681764e9    -- N·m²/C²

return C
