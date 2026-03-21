
## 1-free-particle.lua

- show in 'squiggly line' mode, only helix, no color. this is the wave function
  from the books

- explain graph: horizontal is x axis, vertical is the value, we'll learn later
  what that means

- start time, show it move in space. let it run at a slow pace during the rest
  of this part.

- Explain it's actually just a long row of numbers on a line; enable surf mode
  to show the spokes

- rotate into 3d view, show the spokes are actually complex numbers

- back to 2D view to let it sink in

- wavelength = momentum, short wave length = fast, long wavelength = slow

- enable envelope, disable helix. Show the gaussian, explain about probability
  of 'finding' particle



## 2-standing-still.lua

- show what happens when a particle stands still. When it knows where it is, it
  can't stay still.


3-reflection.lua

- show helix + envelope

- this is what happens when the particle hits a wall; it reflects back
  and interferes with itself. The envelop shows you there are places where you
  simply can not find the particle.

- while boucning, do a deep zoom into the particle penetrating the wall,
  exponentially falling off, it can't make it through



## 4-tunneling.lua

- helix + envelope

- Show full transition at normal speed

- this is a half mirror

- again, zoom in at the boundary, show how the waves leak through

- show that the amplitude is now spread over two bumps; the particle is not in
  two places, it's one wave function with amplitude in two regions. the
  particle can either be found here or found there, but never both or in
  between.


## 5-mach-zehnder.lua

- 2D grid overview + x marginals

- this experiment combines what we have seen before, but now in 2D. One particle
  going through the beam splitter. both halves go their own ways and bounce of
  the mirrors, then they meet again at the beam splitter

- here something remarkable happens: both halves don't *remember* each other,
  they were never apart. thet recombined, constructively in one direction and
  destructively in the other, resulting in only one version of itself


## 6-which-path-1d.lua

- two marginals vertically stacked

- tell the story: particle A goes through a beam splitter. there are now two
  bumps. One bump travels left never to be seen, the right one bumps into
  particle B.

- something remarkable happens here: B gets kicked away by one halve of A, but
  it also gets *not* kicked away by the other half. B is in superposition of
  both being kicked and not kicked. This shows the probabilities of where you
  will find B.

- switch to slice mode, show the "this is what A looks like from B" and reverse.


## 7-which-path-interferometer.lua

- horizontal split, 2d grid P1 large, 2D grid P2 small

- first run with P2 in the middle, on top of the half mirror. Same outcome
  as 5. we have seen this

- move B to the left on the mirror surface, run again. show the outcome: A
  now is all over the place

- Pièce de résistance:: with this outcome on screen, move around B to show
  all the possible outcomes for all possbile postions of B: this is what
  the pattern would have looked like if B were here. Then: move around A
  to show that in one case B would have been undisturbed, vs B had been
  kicked around. These were not two different runs, this was the exact
  same thing happening, we were just looking from a different perspective.



## Closing

This is why we do not see quantum mechanical effects in real life; every photon
is a detector, every air molecule is a detector. Every interaction carries away
a tiny bit of the information, kills the interference with a thousand cuts. In
real life at room temperature there are billions of these interactions every
second; the quantum interference is still there, all in one big wave function,
but it now includes the room, the air, and you.

The more a quantum system touches the world around it, the faster it loses its
quantum signature. Isolation preserves it. Interaction destroys it. That's why
quantum computers need to be colder than deep space.


