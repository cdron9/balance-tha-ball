# 🔴 balance-tha-ball

A physics-based balancing game written in C with SDL3, where your **phone acts as a wireless gyroscope controller**. Tilt your phone to tilt the beam — keep the ball from falling off.

![C](https://img.shields.io/badge/language-C-blue) ![SDL3](https://img.shields.io/badge/library-SDL3-orange) ![UDP](https://img.shields.io/badge/input-UDP%20%2F%20Gyro-green)

---

## How It Works

The desktop app runs an SDL3 window and opens a **UDP socket on port 12345**. Your phone streams gyroscope data (pitch and roll) as JSON over the local network. The app parses the incoming roll value, tilts a beam on screen accordingly, and simulates a ball rolling under gravity on that beam.

```
Phone (gyro) → JSON over UDP → Desktop (C/SDL3) → tilted beam → ball physics
```

---

## Features

- Phone-as-controller via UDP — no cables, no extra software
- Real-time gyroscope input parsed from JSON (`motionRoll`, `motionPitch`)
- Rotated beam rendered with SDL3's `SDL_RenderTextureRotated`
- Ball physics: gravity, friction, slope-based acceleration
- Collision detection in local (beam-relative) space with correct coordinate transforms back to world space
- Non-blocking UDP socket so the game loop never stalls waiting for input
- Target 120 FPS with delta-time based physics

---

## Requirements

- Linux or macOS (uses POSIX sockets — `sys/socket.h`, `fcntl.h`)
- [SDL3](https://github.com/libsdl-org/SDL)
- A phone with an app that can stream gyroscope data as JSON over UDP (e.g. [Sensor Logger](https://github.com/tszheichoi/awesome-sensor-logger), [Physics Toolbox](https://www.vieyrasoftware.net/), or a custom shortcut/app)
- Both devices on the **same local network**

---

## Building

```bash
make
```

This compiles `main.c`, `parse.c`, and the bundled `cJSON.c` into a single binary called `control`.

The makefile assumes SDL3 is installed via Homebrew (`/opt/homebrew`). If your SDL3 lives elsewhere, update `CFLAGS` and `LDFLAGS` accordingly:

```makefile
CFLAGS  = -Wall -Wextra -I/your/sdl3/include
LDFLAGS = -L/your/sdl3/lib -lSDL3
```

To clean:

```bash
make clean
```

---

## Running

```bash
./control
```

The server starts listening on **UDP port 12345** on all interfaces. You'll see pitch/roll values printed to stdout as packets arrive:

```
Server listening on port 12345...
Pitch: -0.031400 | Roll: 0.142300
Pitch: -0.031200 | Roll: 0.198500
```

### Setting Up Your Phone

Configure your gyro-streaming app to send JSON to your desktop's local IP address on port **12345**. The app expects packets in this format:

```json
{"motionPitch": "0.031400", "motionRoll": "0.142300"}
```

> Values are strings (not numbers) — the parser uses `atof()` to convert them.

You can find your desktop's local IP with `ip a` (Linux) or `ifconfig` (macOS).

---

## Project Structure

```
.
├── main.c      # Game loop, SDL3 rendering, UDP socket, ball + beam logic
├── parse.c     # JSON parsing — extracts pitch and roll from incoming packets
├── parse.h     # MotionData struct definition
├── cJSON.c     # Bundled cJSON library
├── cJSON.h
└── makefile
```

---

## Physics Overview

### Gravity & Friction

Every frame, vertical velocity accumulates from gravity and horizontal velocity decays by a friction factor:

```c
ball->vx *= FRICTION;           // 0.999 — slow horizontal slide
ball->vy += GRAVITY * 100 * dt; // pull down
```

### Collision & Slope Force

Collision is handled in **beam-local space** — the ball's position is rotated into the beam's coordinate frame, checked against the beam's AABB, and repositioned on top if colliding. The slope force (what makes the ball roll when you tilt) is derived from the beam angle:

```c
float slopeAcceleration = GRAVITY * 150.0f * sinf(angleRad);
ball->vx += slopeAcceleration * deltaTime;
```

At 0° tilt, `sin(0) = 0` — no force. At full tilt, the force maxes out. The corrected position is then transformed back to world space.

---

## Configuration

Key constants in `main.c`:

| Constant | Default | Description |
|----------|---------|-------------|
| `PORT` | `12345` | UDP port to listen on |
| `TARGET_FPS` | `120` | Target frame rate |
| `GRAVITY` | `9.8f` | Gravity constant (scaled ×100 in physics) |
| `FRICTION` | `0.999f` | Per-frame horizontal velocity decay |

Beam and ball properties are set inline in `main()` and can be tuned directly:

```c
float beamWidth  = 500.0f;
float beamHeight = 10.0f;
Ball ball = {480.0f, 200.0f, 0.0f, 0.0f, 20.0f, {255, 0, 0, 255}};
```

---

## License

MIT — do whatever you want with it.
