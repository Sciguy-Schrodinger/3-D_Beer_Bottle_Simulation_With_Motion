# Beer Bottle Simulation

A small interactive 3D OpenGL beer bottle inspired by the new GTA6 trailer:

drag a beer bottle around, throw it,
watch it bounce off the walls, and shatter into physical shards when it
hits the floor. Liquid sloshes inside the bottle with rising bubbles, and
a puddle spreads at the impact location.

Built from scratch in C++ with GLFW, GLEW, GLM, and stb_image. No game
engine, no framework — just raw OpenGL 3.3 core.

## Controls

| Input | Action |
|---|---|
| **Left mouse button (drag)** | Move the bottle |
| **Release** | Throw it |
| **R** | Reset the scene |

## Features

- Textured bottle quad with dynamic liquid mesh and bubble particle system
- Mouse-drag throw with velocity capture and gravity-driven projectile motion
- Wall and floor collision with damping
- Bottle shatters on floor impact into a grid of physical shards
- Each shard has its own velocity, angular velocity, gravity, and floor rest state
- Liquid puddle rendered at the impact location
- Reset via `R` restores the bottle and clears the shards

## Build

### Dependencies

- OpenGL (3.3 core or later)
- GLEW
- GLFW 3
- GLM
- [stb_image](https://github.com/nothings/stb) (single header, in `stb/`)

On Debian / Ubuntu:

bash
sudo apt install build-essential libgl1-mesa-dev libglew-dev libglfw3-dev libglm-dev

On macOS (Homebrew):

bash
brew install glew glfw glm

### Compile

bash
g++ 3D_beer_bottle_simulation.cpp -o a.out -lGLU -lGL -lGLEW -lglfw

Or with a `Makefile`:

bash
make
./a.out

## Assets

One texture must be present next to the binary:

- `beer_bottle.png` — intact bottle sprite (RGBA, alpha for the silhouette)

must have an alpha channel so the transparent background is discarded
in the fragment shader.

## How it works

The bottle is a textured quad drawn with a small perspective camera. Every
frame, a model matrix places it in world space, using a pivot at the
bottle's visual center so that rotation looks natural.

While dragging, the mouse position updates `dx`/`dy` and captures a
velocity. On release, the bottle becomes a projectile and `dx`/`dy`
integrate under gravity. When it crosses a wall or the floor, the
corresponding velocity component is reversed and damped.

On floor impact, the bottle's `broken` flag flips, and a set of shards is
spawned. The shards are a 10×10 grid of triangles that exactly tile the
bottle quad; each shard's centroid is transformed by the same model matrix
the bottle used at impact, so the shards appear to originate from the
bottle's exact position and orientation. Each shard then receives a
velocity directed away from the bottle's pivot, plus spin. A simple
integration loop applies gravity, drag, and a floor rest condition that
zeroes velocity once the shard is nearly stationary.

The liquid inside the bottle is a small cylindrical mesh whose top surface
is modulated by a sine wave. A point-sprite particle system renders
bubbles that rise and wrap around. On break, the liquid mesh is replaced
by a flat irregular puddle placed at the impact x-position.

## Project layout
.
├── 3D_beer_bottle_simulation.cpp   # everything: shaders, geometry, physics, main loop

├── beer_bottle.png

├── stb/

│   └── stb_image.h

├── Makefile

└── README.md

## Notes

- Shader sources are embedded as string literals inside the `.cpp`. If you
  want to edit them separately, extract them to `shaders/*.vert` and
  `shaders/*.frag` and load them at runtime.
- The simulation has no sound. A single crash sample triggered on the
  break would add a lot.
- The camera is fixed. Orbiting or zooming would be a natural extension.

## Credits

- Libraries: GLEW, GLFW, GLM, stb_image
- Textures: replace with your own, or credit the source here

## License

MIT — see `LICENSE`.
