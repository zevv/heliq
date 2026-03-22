
# The Wave Function Is Real

Presenter notes. Live simulator, projector, ~15 people.
Audience: scientifically curious, read popsci, know the terminology,
never went deep on the math.


## Opening

Show the Schrodinger equation on screen. Pause.

"I've been reading about quantum mechanics for twenty years. And for twenty
years I stared at this equation and didn't know what to do with it. It's a
differential equation — it doesn't *do* anything. It just sits there and
describes a relationship.

But if you squint at it the right way, it says something very simple: given
the state of the system right now, here's how it changes in the next instant.
And *that* is something I can put in a loop. Take the state at time zero,
apply the rule, get time one. Apply it again, get time two. A million times
per second on a GPU. What you're about to see is nothing more than that —
one formula, applied over and over. Everything that happens, happens because
of that one rule."


## 1-free-particle.lua — The Wave Function

Start paused. Helix only, no surface, no color.

"This is a wave function. Not an artist's impression — this is the actual
data. 512 complex numbers on a line, representing one electron."

Point out the axes: horizontal is position in space (nanometers), vertical
is the value. Don't explain what the value means yet — just let them see it.

Start time. Let it drift across the screen at a slow pace.

"It moves. It spreads. This is a free electron at about 0.1 electron volts —
the energy scale of nanoelectronics."

Enable surface/spokes to show the discrete samples.

"What looks like a smooth curve is actually just a row of numbers. Each spoke
is one complex number — a point on the grid."

Rotate into 3D. The helix becomes visible as a spiral.

"This is why it's called a wave function. Each value has a real and imaginary
part. Plotted in 3D, it's a corkscrew. The wavelength of that corkscrew
*is* the momentum of the particle. Short wavelength means fast. Long means
slow."

Back to 2D. Enable envelope, disable helix.

"This bump is the probability amplitude. Square it and you get the probability
of finding the particle at each position. The electron isn't *at* a point —
it's spread out, and this shape tells you where you're likely to find it."


## 2-standing-still.lua — Uncertainty

"What happens when a particle sits perfectly still? Zero momentum."

Load and run. The packet spreads immediately.

"It can't stay still. The more precisely you pin down where it is, the more
uncertain its momentum becomes — and all those momentum components fly apart.
This is Heisenberg's uncertainty principle — not a problem with our instruments,
not about clumsy measurements disturbing the particle. It's structural. The
wave function *cannot* be both narrow in position and narrow in momentum. It's
not that we don't know — it's that there's nothing to know. This is the
equation, doing its thing."


## 3-reflection.lua — Hitting a Wall

Helix + envelope.

"Now there's a wall in the way. Watch what happens."

Let it hit the barrier and reflect. Point out the interference pattern forming
as the reflected wave overlaps the incoming wave.

"The envelope shows something beautiful: during the collision there are places
where the probability drops to zero. The particle literally cannot be found
there. That's interference — the reflected wave canceling the incoming wave
at specific points."

Zoom into the barrier edge to show the evanescent tail leaking into the wall.

"Even though it bounces back, look: it leaks into the wall. The amplitude
decays inside the barrier. It can't make it through — the wall is too thick
and too tall — but it tries."


## 4-tunneling.lua — The Half Mirror

Helix + envelope.

"Same setup, but now the barrier is thinner and lower. Tuned so that exactly
half the wave gets through."

Run at normal speed. Watch the packet split into two bumps — one reflected,
one transmitted.

"This is quantum tunneling. The particle hit a wall and half of it leaked
through. But here's the thing: there is still only one electron. We haven't
split anything. The wave function now has amplitude in two regions — and if
you go looking for the particle, you'll find it on one side or the other.
Never both. Never in between."

Zoom into the barrier during the split to show the wave leaking through.

"This is a half mirror for electrons. It's how tunnel diodes work, how
scanning tunneling microscopes image individual atoms. Not a metaphor — this
is the actual mechanism."


## 5-mach-zehnder.lua — The Interferometer

2D grid view on top, helix in marginal mode below.

"Now we go to two dimensions. Same electron, but it can move in a plane.
There's a beam splitter — a thin barrier, same tunneling principle — plus
two mirrors."

Run. The packet hits the beam splitter, splits into two paths, each bounces
off a mirror, and they reconverge at the splitter.

"Watch what happens when the two halves meet again."

They recombine. One output port gets everything, the other gets nothing.

"The two halves were never apart. They were always one wave function,
accumulating phase along two different paths. When they meet, the phases
line up constructively in one direction and destructively in the other.
The particle *must* exit to the right. Not 50/50 — 100/0. The wave
function guarantees it."


## 6-which-path-1d.lua — Two Particles

Two helix widgets, marginal mode, stacked vertically — one for each particle.

"Now there are two electrons. Two particles means two axes — the horizontal
axis of the top plot is particle A's position, the bottom plot is particle
B's position. This is no longer physical space — it's configuration space."

Run. A hits the beam splitter and splits. The transmitted half reaches B
and kicks it via a short-range interaction.

"A split in two. The right half bumped into B and kicked it. But the left
half didn't reach B — so from B's perspective, nothing happened. B is now
in a superposition: kicked and not kicked, at the same time."

Point at B's marginal — it shows two peaks.

"B's state now depends on what A did. If A went right, B got kicked. If A
went left, B is still sitting there. They are entangled — you cannot
describe one without the other."

Switch to slice mode. Move A's cursor.

"When I pick a position for A on the right side of the barrier — B is over
here, kicked. When I pick a position for A on the left — B hasn't moved.
Same wave function, same single run. I'm just looking at different slices
of the same object."


## 7-which-path-interferometer.lua — The Payoff

2D grid, showing A's x-y plane.

"Back to the interferometer. Same mirrors, same beam splitter. But now
there's a second particle — a heavy detector — sitting off to the side,
away from both arms."

First run: B's cursor centered, away from the action.

"B is out of the way. A does its thing — splits, bounces, recombines.
Same result as before: everything exits one port."

Pause. Reload with R. Move B's cursor onto the left arm, between the
beam splitter and the mirror.

"Now B is on the left arm. Run it again."

Second run: the output is different — A's probability is split between
both ports.

"The interference is gone. B interacted with one arm of the interferometer.
That interaction entangled them — B now carries information about which
path A took. And that information, just by *existing*, is enough to destroy
the interference pattern."

Now the punchline. Pause the simulation.

"But here's what really gets me. This was one simulation. One wave function
evolving under one equation. Watch what happens when I move B around."

Drag B's cursor slowly from the arm position back to the center and beyond.
The pattern on A's grid morphs continuously — from split output through
to clean recombination and back.

"Every position of B gives a different answer for A. This isn't multiple
runs — it's one mathematical object, and I'm slicing through it. Where
you look determines what you see."

Move A's cursor to show the reverse: for each output of A, B is in a
different state.

"And it works both ways. Pick where A ended up — now B's state changes.
They are one thing. They were always one thing. The equation made them
one thing."


## Closing

"One thing I keep learning, over and over. In school they tell you: you
can't divide by zero. Then calculus shows up and it turns out you can —
you take a limit, and suddenly everything about rates of change becomes
simple. They tell you: you can't take the square root of minus one. Then
complex numbers show up, and polynomials and rotations and signal
processing all become *cleaner* than they were before.

The pop-science version of quantum mechanics does the same thing. It
tells you: two particles each have their own wave function, and then
something magical called entanglement happens and they share one. When
does that happen? Is it instant? Gradual? Can they half-share a wave
function, like being a bit pregnant?

The answer is: you were lied to. There were never two wave functions.
There was always one — over the combined configuration space of both
particles. When they don't interact, that one wave function happens to
be factorable, so it *looks* like two separate things. The moment they
interact, the factorization breaks. That's all entanglement is. Not
something new that appeared. A simplification that stopped working.

You just saw this on screen. The grid didn't change. The axes didn't
change. The wave function was always one object. It just stopped being
separable.

Every time I hit one of these walls — every time the real explanation
replaces the simplified lie — things got simpler, not harder. The truth
is more elegant than the shortcut."

Pause.

"Everything you just saw came from one equation, applied a million times.
No special cases, no if-statements for tunneling or interference or
entanglement. All of it emerges from the same update rule.

I've spent hundreds of hours on this and I still don't understand what
it means. I don't think anyone does. But seeing it move, seeing the wave
split and recombine and entangle — it stopped being abstract for me. The
squiggly line in the books is real. It does things. And the things it
does are stranger than any pop-science article can convey."

