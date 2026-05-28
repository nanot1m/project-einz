# project-einz

Terminal-based ECS shooter written in C++ as a learning project.

**▶ Play in browser:** <https://nanot1m.github.io/project-einz/>

Also runs in a POSIX terminal (macOS / Linux). Single codebase — only the backend (terminal vs wasm) differs.

## Features

- **ECS-style architecture** — `Entity` is identity (`id`, `type`, `in_use`); all properties (`positions`, `velocities`, `hps`, `speeds`) are parallel component arrays indexed by entity id.
- **Friction-based player movement** — impulse on keypress, exponential damping, max-speed clamp.
- **Tile-sweep bullet collision** — bullets check every tile along their path each frame; no teleporting through enemies.
- **Three enemy variants** — regular (`&`), strong (`M`, 2 HP), swift (`s`, faster); each randomly spawned.
- **Dijkstra/BFS pathfinding** — single distance field computed from the player every 200ms; enemies follow the gradient.
- **Terrain generation** — random lakes (ellipses), two rivers (random-walk drift) with bridges that connect both sides.
- **Trees** — 2×3 ASCII Christmas trees, block movement and bullets.
- **Pickups** — grass (`"`, +1-2 ammo, common), ammo packs (`=`, +5, rare).
- **Player health + invincibility frames** — 3 HP, 1s i-frames after hit.
- **Game-over screen** with score and restart prompt.
- **Cell-grid IR + reconciler render** — the frame is built into an intermediate cell grid, then only diffs vs. the previous frame are emitted to the terminal. Static walls/floor/HUD labels render once.
- **Run-length color encoding** — color escape sequences emitted only on color change; cells of the same color form long runs.
- **Hybrid sleep + spin-wait** for accurate 120fps.
- **Alternate screen buffer** — terminal restored cleanly on exit (q, Ctrl+C, atexit).

## Controls

| Key            | Action                         |
| -------------- | ------------------------------ |
| Arrow keys     | Move player                    |
| `Space` or `J` | Shoot in facing direction      |
| `R`            | Restart (only on Game Over)    |
| `Q` or Ctrl+C  | Quit                           |

## Build & Run

Via the Makefile:

```bash
make            # build terminal binary (./einz)
make run        # build + run terminal binary
make pages      # build the browser bundle into ./docs
make web        # build + serve at http://localhost:8000/
make clean      # nuke all build artifacts
```

Manual commands if you want them:

```bash
# Terminal (POSIX — macOS / Linux)
clang++ -std=c++17 -O2 project-einz.cpp -o einz

# Web (emscripten)
emcc wasm-einz.cpp -o einz.html --shell-file shell.html -O2 \
     -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_RUNTIME_METHODS=HEAP8
```

Both builds share `engine.hpp` + `platform.hpp` — only the backend (`terminal.hpp` vs `wasm.hpp`) and the entry-point `.cpp` differ.

## Tuning

All gameplay constants live in `namespace cfg` at the top of [project-einz.cpp](project-einz.cpp). Edit a value, recompile, observe.

```cpp
constexpr float PLAYER_IMPULSE         = 16.0f;
constexpr float PLAYER_MAX_SPEED       = 16.0f;
constexpr float BULLET_SPEED           = 40.0f;
constexpr float ENEMY_REGULAR_SPEED    = 2.5f;
constexpr int   SCORE_PER_KILL         = 10;
constexpr float DAMPING                = 20.0f;
// ... etc
```

## Glyphs

| Symbol | Entity         | Color        |
| ------ | -------------- | ------------ |
| `@`    | Player         | bold blue    |
| `&`    | Enemy regular  | bold red     |
| `M`    | Enemy strong   | bold magenta |
| `s`    | Enemy swift    | bold yellow  |
| `>` `<` `^` `v` | Bullet (directional) | bold white |
| `*`    | Hit effect     | bold yellow  |
| `"`    | Grass pickup   | bold green   |
| `=`    | Ammo pack      | bold yellow  |
| `/\` `||` | Tree (2×3)  | bold green   |
| `▓`    | Wall           | dim gray     |
| `~`    | Water          | bold cyan    |
| `_`    | Bridge         | brown        |
| `.`    | Floor          | dim gray     |
| `♥` `♡` | HP heart full/empty | red / gray |

## Files

| File | Role |
| ---- | ---- |
| `platform.hpp` | Shared ABI: `DynamicArray`, `Key`, `Cell`, `RenderFrame`, abstract `Renderer` / `InputSource`. |
| `engine.hpp` | All game logic: `cfg`, enums, `Entity`, `World`. Depends only on `platform.hpp`. |
| `terminal.hpp` | POSIX backend + `run_terminal()` main loop (sleep_until, SIGINT). |
| `project-einz.cpp` | POSIX entry: builds `World`, calls `run_terminal`. |
| `wasm.hpp` | WebAssembly backend + `run_wasm()` (registers a `requestAnimationFrame` callback). |
| `wasm-einz.cpp` | Wasm entry: builds `World`, calls `run_wasm`. |
| `shell.html` | HTML shell used by emcc — owns the DOM grid + JS-side diff cache + keyboard listener. |

## Architecture overview

```
main loop
 ├─ read_key  (non-blocking, raw mode)
 ├─ world.handle_input(key)
 ├─ world.update(dt)
 │   ├─ compute_pathfinding  (BFS, every 200ms)
 │   ├─ spawn_system         (timers)
 │   ├─ enemy_ai_system      (gradient descent on dist_field)
 │   ├─ speed_limit_system
 │   ├─ movement_system      (per-axis, wall slide)
 │   ├─ bullet_system        (tile sweep, wall/enemy hit)
 │   ├─ pickup_system
 │   ├─ enemy_damage_system  (i-frames)
 │   ├─ effects_system       (hit flash decay)
 │   └─ friction_system      (linear damping)
 └─ world.render(fps)
     ├─ build_cells          (game state -> IR)
     └─ emit_diff            (IR vs prev -> ANSI commands)
```

## License

No license declared — public for learning/reference. Adapt as you wish.
