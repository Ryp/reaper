# Physics Port Plan — `Neptune::PhysicsSim` → Zig

*Planning only; no code written yet. Post-v1, on `feature/zig-port`. Companion to `ZIG_PORT_PLAN.md`.*

## Recommendation

**Do not link Bullet. Reimplement the used subset in Zig (~700–900 lines), with no C++ added to the link.**

The sim is not a physics-engine consumer in any meaningful sense. It is a hand-rolled arcade vehicle
controller that borrows five small things from Bullet, each between 3 and 60 lines. The sixth thing —
box-vs-trimesh contact manifolds and the impulse solver — is the only expensive piece, and it is deferred
behind a measurement rather than assumed away.

### Why Bullet contributes almost nothing today

Bullet's dynamics are *structurally cancelled* for everything except contacts, and the chain is short
enough to verify by reading:

- `btDiscreteDynamicsWorld.cpp:196` — the world defaults to gravity `(0, -10, 0)`.
- `create_sim` never calls `setGravity` — confirmed, `grep setGravity src/neptune/` is empty.
- `btDiscreteDynamicsWorld.cpp:430` — `applyGravity()` runs once, *before* the substep loop.
- `btDiscreteDynamicsWorld.cpp:452-460` — the pre-tick callback fires at the *top* of each substep.
- [PhysicsSimUpdate.cpp:67](src/neptune/sim/PhysicsSimUpdate.cpp:67) — that callback's third statement is
  `player_rigid_body->clearForces()`.

So Bullet's world gravity never reaches `integrateVelocities`. Gravity is applied by hand at
[PhysicsSimUpdate.cpp:142](src/neptune/sim/PhysicsSimUpdate.cpp:142) from a per-track-chunk frame — which
a scalar world gravity structurally cannot express anyway. Thrust, brake, steering torque, damping,
respawn and the whole suspension are hand-applied too. There is no `btRaycastVehicle`
(`grep RaycastVehicle src/` is empty); the code reimplements it inline, keeping `btWheelInfo`'s variable
names verbatim.

What is genuinely Bullet's:

| Capability | Source to transliterate | Cost |
|---|---|---|
| Damping | `btRigidBody.cpp:158` — `v *= pow(1 - d, dt)`, `d` clamped to `[0,1]` at `:143` | 4 lines |
| Angular clamp | `btRigidBody.cpp:380` — `MAX_ANGVEL = π/2` per step | 5 lines |
| Box inertia | `btBoxShape.cpp:38` — `I = m/12 · (l² + l²)`, `l = 2·half_extent` | 5 lines |
| `getVelocityInLocalPoint` | `btRigidBody.h:460` — `v + ω × rel` | 1 line |
| `applyImpulse` | `v += imp·invMass`; `ω += I⁻¹·(rel × imp)` | 3 lines |
| Ray vs triangle | `btRaycastCallback.cpp:36-116` — **not** Möller-Trumbore | ~45 lines |
| Closest-hit callback | `btCollisionWorld.h:246-264` | ~20 lines |
| **Box-vs-trimesh manifold + solver** | `btSequentialImpulseConstraintSolver` | **1–3k lines** — deferred |

### Options rejected

**Link Bullet's C++ from `external/` (the VMA/lodepng pattern).** The pattern does not scale here.
`addMeshOptimizer` enumerates ~25 files; the three Bullet libraries are **385 files / ~120k lines**, and
`cmake/external/bullet.cmake` gets them via `add_subdirectory` of Bullet's own 250-line CMakeLists with a
dozen options — there is no source list to copy, so build.zig would need a hand-maintained 385-entry array
that drifts on every submodule bump. `build.zig.zon` already declares `external/bullet3` CMake-only.

**Use a Bullet C API.** Dead end, verified: no `Bullet-C-Api.h` in 3.25, no `plCreateDynamicsWorld` symbols,
`external/bullet3/src/` exposes only two C++ headers. The `*C_API*` files under `examples/SharedMemory/` are
PyBullet's IPC protocol and are not built. Any C access means *writing* a ~35-function shim — which buys
nothing over linking directly and still drags in all 385 files.

**Use a Zig physics library** (`zphysics`/Jolt, `zbullet`). Rejected three times over: they are C/C++ behind
a binding so the link cost is unchanged; Jolt is not Bullet, so operation-for-operation transliteration is
impossible and the golden-trace gate below cannot exist; and `zbullet` pins a 2.89-era engine.

### The one thing that could change the answer

**How often does the player box actually generate a contact manifold?** Nobody has measured this. The track
chunk is a tube (`y[-0.2, 0.4] z[-1.3, 1.3]`, 2.6 units wide) and the ship is 0.8 × 0.4 × 0.7 — it fits, but
wall scrapes are plausible in normal driving. Bullet stops the ship going through the wall; a first Zig cut
will not. M9.0 records a per-substep contact count precisely to settle this before any Zig is written.

Fallback ladder if contacts turn out frequent (>5% of substeps): sphere-vs-triangle pushout with no solver
(~80 lines, reproduces "does not go through walls" without reproducing Bullet's impulses), then a box-vs-
triangle SAT manifold plus projected Gauss-Seidel (~500–800 lines). Reverting to linking Bullet stays
possible throughout, because the collision world sits behind one function.

## File layout

New directory `src/neptune/sim/` — five files rather than a flat `sim.zig`:

| File | Mirrors |
|---|---|
| `sim.zig` | `PhysicsSim.h` + `.cpp` — types, create/destroy, `getPlayerTransform`, colliders |
| `sim_update.zig` | `PhysicsSimUpdate.cpp` — `updateForces`, `resetPlayerPosition`, `simUpdate` |
| `rigid_body.zig` | the used parts of `btRigidBody.{h,cpp}`, each with a `// Mirrors btRigidBody.cpp:NNN` |
| `collision_world.zig` | `btCollisionWorld::rayTest` + `btTriangleRaycastCallback` + closest-hit |
| `trace.zig` | test-only golden-trace record layout and reader |

`BulletConversion.inl` gets no Zig counterpart — with Bullet gone there is nothing to convert, and its
row-major/column-major hazard evaporates. Worth a note at the top of `sim.zig` so nobody goes looking.

**`build.zig` needs no changes at all**, beyond adding the new modules to `src/test.zig`'s import list.
That is the strongest one-line argument for this approach.

## Milestones

Everything through M9.5 is CPU-only and runs under `zig build test` with no Vulkan device — which is most
of the work, matching `src/test.zig`'s existing GPU-free contract.

### M9.0 — Golden trace harness (C++ only)

The keystone: determinism makes exact-value testing possible, and this milestone manufactures the reference
values. Add a `--sim-trace <path>` mode to the C++ build with a fixed trackgen seed, a **scripted input
table** replacing the controller read, and a **fixed dt** — three traces at `dt = 1/60`, `1/30`, and `0.2`
(the last clamped to 3 substeps, exercising Bullet's discard-the-surplus branch).

Dump three artifacts: `sim_track.bin` (the 100 `TrackSkeletonNode`s, ~28 KB — this decouples sim equivalence
from trackgen RNG equivalence, which the Zig port cannot and should not reproduce); `sim_trace.bin` (one
record per substep with transform, velocities, `closest_skeleton_node`, `gravity_frame`, forces, and per
suspension the hit fraction, normal, denominator, force and impulse — **plus the contact manifold count**);
and `sim_skinned_checksum.txt` (per-chunk FNV-1a over skinned vertex positions).

**Gate:** two runs produce byte-identical traces. If that fails, the C++ sim is not deterministic and
everything downstream is void — find out now, not at M9.5.

### M9.1 — Math seam and trackgen cross-validation

Add to `src/math/linalg.zig` only what is missing: `distance2`, `proj`, `mulMat4x3`, `mat3FromMat4x3`,
`inverseMat4x3`, inertia helpers. `Mat3`, `Mat4x3`, `normalize`, `dot`, `cross` already exist and already
match Bullet bit-for-bit on this platform (see below).

Separately, a test that runs Zig skinning on `sim_track.bin`'s exact nodes and compares per-chunk checksums.
If Zig skinning diverges by one ulp the collision geometry differs, the rays hit different triangles, and
M9.5 looks like a sim bug when it is a mesh bug.

**Gate:** `zig build test`; checksum equality on all 100 chunks, exact.

### M9.2 — Data model

Transliterate `PhysicsSim.h:41-134` field for field, in order. The Bullet members collapse into a Zig-side
`RigidBody` plus a collider map. Delete `sim_start` — its only job was installing the tick callback, and
with the substep loop written explicitly there is no callback.

**Gate:** tests asserting all 15 `Vars` defaults literal by literal against `PhysicsSim.cpp:38-52`, and the
four suspension attach points in construction order.

### M9.3 — Static colliders and the raycast world

`rayTest` transliterates three Bullet layers: per-collider AABB slab reject; per-triangle
`btRaycastCallback.cpp:36-116` **literally, not Möller-Trumbore**; closest-hit accumulation.

No BVH. Budget is 4 rays × ≤3 substeps = **12 rays/frame** against ~130k triangles in 100 chunks, and a
per-collider AABB reject leaves 1–2 chunks per ray. Adding `btOptimizedBvh`-equivalent quantization is pure
risk with no payoff, and it changes triangle visit order, which changes which of several coplanar hits wins.

**Gate — the highest-value gate in the plan.** Hand-built single-triangle tests (front hit, back hit with a
flipped normal, exact-edge hit, ray ending on the plane must miss, fraction exactly 1.0 must miss), then
replay `sim_trace.bin`'s **raycasts only** and assert hit fraction to 1e-6 and normal to 1e-5. This isolates
the most divergence-prone subsystem from all the dynamics.

### M9.4 — `updateForces`, one substep, replayed

Transliterate `PhysicsSimUpdate.cpp:57-284` statement for statement.

**Gate — the one-substep oracle.** For each trace record, seed a Zig sim with that record's exact input
state and run *one* substep. Because each substep starts from ground truth, errors cannot accumulate and any
mismatch points at exactly one formula. Assert `closest_skeleton_node` as an exact integer, `gravity_frame`
to 1e-6, forces to 1e-5. Report the first failing substep index.

### M9.5 — Integrator and accumulator

Transliterate `btDiscreteDynamicsWorld.cpp:386-449`. The accumulator subtracts the **unclamped** substep
count, so surplus time above the cap is discarded — that is the spiral-of-death guard, and banking the time
changes behaviour. It must be **f32**: `btScalar` is float (`external/bullet3/CMakeLists.txt:26`,
`USE_DOUBLE_PRECISION OFF`), and using f64 is the easiest way to silently desync every trace.

**Gate — closed-loop replay.** Assert substep count per frame matches exactly for all three dt values (this
is what catches the f32 accumulator bug); report the first substep where position diverges past 1e-4 and
assert it beats a committed ratchet; assert no NaN and all suspension values in range.

Bit-exactness is **not** the gate and should not be attempted — glm vs `linalg`, GCC vs LLVM, and FP
contraction each cost a few ulp into a chaotic system. The divergence-onset ratchet is the honest substitute.

### M9.6 — Scene integration

- `src/renderer/scene.zig:170` — `Track` is currently freed at the end of `createGameScene`, so
  `skeleton_nodes` dies immediately. Move it into `Scene`. **This is a use-after-free waiting to happen** the
  moment the sim borrows it.
- `src/renderer/scene.zig:158` — call `simCreateStaticCollisionMeshes` *before* `loadMeshes`; the colliders
  deep-copy, so only ordering changes.
- `src/game_loop.zig:154` — build `ShipInput` from the controller axes (which already exist and already feed
  the Controller Axes window; they just stop being decorative), call `simUpdate`, write
  `scene.player_node.transform_matrix`. Everything parented to the player node — camera, helmet, light —
  then moves for free.
- `src/game_loop.zig` has no frame dt today; v1 never needed one.

**Deviation to decide explicitly:** `get_player_transform` reads the *motion state*, which Bullet interpolates
by the leftover accumulator time, while `update_forces` reads the raw transform. Rendering and simulation
deliberately see different transforms. Recommend reimplementing the interpolation — without it the ship
visibly stutters at 144 Hz against a 60 Hz substep, and that reads as a regression.

**Gate (no GPU):** add `--fixed-dt` and `--sim-dump`; run twice and `cmp`. Plus assert the settled state —
ship Y stable to 1e-4 over the last 60 substeps, all four suspension ratios converged and equal to each other
(the ship is symmetric and neutral input means `throttle = brake = 0`).

### M9.7 — The Physics window stops being a stub

Delete `PhysicsSimVars` (`src/renderer/debug_ui.zig:178-197`) and point the window at `*sim.Vars` directly.
Unflatten `ship_thrust`/`ship_braking`/`ship_handling` back into `default_ship_stats` so the labels stop
lying. Every slider then writes into the live sim, which reintroduces the C++'s mid-accumulation glitch when
`simulation_substep_duration` changes — reproduce it, do not fix it.

### M9.8 — Debug geometry, and the contact decision

Port `create_debug_command_sphere`/`_box` and append to `prepared.debug_draw_commands` (declared, currently
with zero appenders). The three GPU passes are already ported and need no work. This is the only reason
`position_start_ws`, `position_end_ws`, `length_ratio_last` and `last_gravity_frame` exist — nothing in the
sim reads them back, so write them from M9.4 or the window is blank.

Only now, with M9.0's manifold count in hand, decide on contacts.

## Where a transliteration will silently diverge

**Already safe, do not "improve":** on Linux/GCC `BT_USE_SSE_IN_API` is *not* defined (`btScalar.h:222`
defines it only under `__APPLE__`), so `btVector3::normalize()` is a plain reciprocal multiply — bit-identical
to `linalg.zig:64`. On macOS it would use `_mm_rsqrt_ss` plus one Newton step and would *not* match; worth a
comment, as it is a landmine for a future macOS build.

The traps, ordered by cost of getting them wrong:

| Site | The trap |
|---|---|
| `btRaycastCallback.cpp:108` | Default flags are `kF_None`, so backfaces are **not** filtered and the normal **is flipped** when `dist_a <= 0`. Consequently the suspension denominator is always ≤ 0 on a genuine hit, and the `>= -0.1` guard fires only at grazing angles. Get the flip backwards and every suspension force inverts. |
| `btRaycastCallback.cpp:73` | The inside test uses `>= -0.0001·\|n\|²` with `n` **un-normalized** — a size-scaled, deliberately generous edge tolerance. Möller-Trumbore's `u,v >= 0` is stricter and misses shared-edge hits, which on a tessellated tube is many of them. |
| `btRigidBody.cpp:158` | Damping is **exponential**: `v *= pow(1 - d, dt)`. With `angular_friction = 0.999` that is `0.891` per substep; a linear `v *= (1 - d·dt)` gives `0.983` — off by ~16× in retained angular velocity per second. |
| `PhysicsSimUpdate.cpp:148` | `glm::proj` divides by `dot(fwd,fwd)`. Hand-writing `fwd * dot(v,fwd)` assumes `fwd` is exactly unit; it is only nearly unit, so the difference is a slowly drifting brake strength. Port the division. |
| `PhysicsSimUpdate.cpp:264` | Force is scaled by mass **then** clamped to `[0, max_suspension_force]`. Swapping the order changes the clamp's meaning by 2×. The lower bound of 0 means suspension pushes only, never pulls. |
| `PhysicsSimUpdate.cpp:240` | The spring uses the hit **fraction** (normalized to ray length), not distance. Report distance instead and `default_spring_stiffness = 30` silently means something else. |
| `PhysicsSimUpdate.cpp:69` | The nearest-node seed is `10000000.f`, i.e. a ~3162-unit cutoff, **not** `FLT_MAX`. Beyond it gravity snaps to world-down and respawn never fires. Using `FLT_MAX` changes the failure mode from "stuck falling" to "always respawns". |
| `btRigidBody.cpp:380` | `MAX_ANGVEL = π/2` per step. Easy to omit; only shows up in violent spins. |

**Ordering that is load-bearing.** The four suspension impulses are *not* independent — `applyImpulse`
mutates velocity immediately and suspension *n+1* reads the mutated value, so they must run in construction
order (FL, FR, RL, RR); batching them changes handling. `chassis_transform` is read once and deliberately not
re-read between them. Forces and impulses are mixed: gravity/thrust/brake/steer accumulate as forces
integrated later, suspension is a velocity change applied now, and the conversion uses the **substep** dt.

## Risks and open questions

1. **Units are not SI metres.** `src/neptune/trackgen.zig:22` defines `meter_in_game_units = 0.01`, so the
   world is in game units where 1 unit = 100 m, the 300–600 m track radii become 3–6 units, and
   `gravity_force_intensity = 9.8331` is ~983 m/s² taken literally. **This changes nothing for the port —
   transliterate the numbers.** Flagged so nobody "corrects" gravity. It lines up with the known 0.1..100
   light-frustum world scale.
2. **Contact frequency is a guess, not a measurement** — the plan's central unverified assumption, and the
   reason M9.0 exists.
3. **The self-hit hazard is structurally avoided.** The suspension ray starts inside the player box and the
   C++ callback never filters on `body != player`; it works only because Bullet's convex cast does not report
   hits from an interior origin. The Zig port never puts the player in the collider set. Say so at the site,
   or someone will "add the missing filter" and be confused that it is already absent.
4. **Exact float parity is unachievable** and the gates must not assume it. If M9.4's per-substep oracle
   passes at 1e-5 but M9.5 diverges within 50 substeps, that is expected, not a bug.
5. **`toBt(glm::quat)`/`toGlm(btQuaternion)` in `BulletConversion.inl` are buggy** — `btQuaternion(w,x,y,z)`
   against Bullet's `(x,y,z,w)` constructor. They are self-consistent, dead, and untested. Do not port the
   bug; do not port the functions.
6. **Whether the C++ trace run can go headless is unknown** — `GameLoop.cpp` is entangled with the renderer.
   If it cannot, M9.0 needs a GPU once, which is acceptable but should be discovered before scheduling.

## What not to do

- **Do not touch the C++ build** beyond M9.0's trace dumper. It stays alive and stays the reference.
- **Do not build a BVH.** 12 rays/frame against per-chunk AABBs is free.
- **Do not port the `if (false)` righting-torque block** (`PhysicsSimUpdate.cpp:157`) as live behaviour — but
  do not delete it silently either; the C++ still has it.
- **Do not port `btGImpactCollisionAlgorithm::registerAlgorithm`** (`PhysicsSim.cpp:59`). No GImpact shape is
  ever created.
- **Do not implement multi-player.** `sim.players.front()` has a `// FIXME`, but `raycast_suspensions` is
  global to the sim, not per-player — generalizing is a redesign, not a port.
- **Do not implement `default_ship_stats.handling`.** It is never read. Keep the slider, wire it to nothing.
- **Do not "fix" the FIXMEs** — `alloc_number_fixme`, the single-input-per-frame latch, the CCD settings that
  do nothing. Every one of them is a golden-trace difference.
- **Do not implement track regeneration.** Scene nodes are arena-allocated with no destructor and the mesh
  cache does not handle fragmentation. That is a mesh-cache project, not a physics one.
- **Do not write the contact solver before M9.0's manifold count comes back.** It is the largest single chunk
  of work in the plan and it may be unnecessary.

## Effort sketch

| Milestone | Scope | GPU |
|---|---|---|
| M9.0 trace harness (C++) | ~200 lines, one build option | maybe, once |
| M9.1 math + trackgen cross-check | ~80 lines + tests | no |
| M9.2 data model | ~250 lines + tests | no |
| M9.3 colliders + raycast | ~250 lines + tests | no |
| M9.4 `updateForces` | ~250 lines + replay test | no |
| M9.5 integrator + accumulator | ~150 lines + closed-loop test | no |
| M9.6 scene integration | ~120 lines across 4 files | no |
| M9.7 debug window | ~60 lines, mostly deletions | screenshot |
| M9.8 debug geometry (+ contacts TBD) | ~120 lines (+0–800) | screenshot |

Total excluding contacts: **~1,150 lines**, of which ~450 is transliteration with a per-statement C++
reference and ~350 is test scaffolding. Seven of nine milestones need no GPU.
