
## How to build and run

```
make -j$(nproc)
./heliq                                    # default experiment
./heliq experiments/090-2D-1P-double-slit.lua  # specific experiment
```

Dependencies: SDL3, FFTW3 (float), OpenCL, Lua 5.4, Dear ImGui (in external/).

## Controls

- Space: play/pause
- /: reverse time
- R: reload experiment from disk (preserves speed/dt)
- M/N: measure particle on axis 0/1
- Shift+M/N: decohere particle on axis 0/1
- B: toggle absorbing boundary
- P: toggle potentials
- D: dump displayed slice to dump.txt (in focused grid widget)
- A: reset view/controls to defaults (per widget)
- F1/F2/F3/F4: assign info/helix/grid/trace widget to focused panel
- RMB: pan, Shift+RMB: orbit camera (3D widgets), scroll: zoom
- 1/3/7/5: front/side/top/ortho views (numpad or number keys)
- LMB drag: move cursor (grid and helix)
