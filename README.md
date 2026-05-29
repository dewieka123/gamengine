# 🎮 gamengine

> A 2D/3D game engine module for VerseLanguage, powered by Qt6, OpenGL, Assimp, and LibVLC — supporting rendering, audio, video playback, keyboard and mouse input, collision detection, and 3D model loading, all scriptable from a single `.vl` file.

**Part of the [VerseLanguage](https://github.com/dewieka123/verselanguage) ecosystem.**

---

## 🧩 What is gamengine?

`gamengine` is a native shared library (`.dll` / `.so` / `.dylib`) written in C++ that plugs directly into VerseLanguage via the `with` keyword. It gives your `.vl` scripts a full game window with 2D and 3D rendering, audio and video playback, keyboard and mouse input, collision detection, and image backgrounds — all from a clean, simple API that fits the human-first philosophy of VerseLanguage.

No separate IDE. No project files. No boilerplate. Load it, call it, build games.

---

## 🔧 Tech Stack

| Library | Role |
|---------|------|
| **Qt6** — QOpenGLWidget, QPainter | Window management, 2D rendering, input events |
| **OpenGL** | 3D rendering with depth testing |
| **Assimp** | 3D model loading (.obj, .fbx, .gltf, .dae, and more) |
| **LibVLC** | Audio playback and thread-safe in-game video rendering |

---

## 🚀 Quick Start

```
with "module\gamengine.dll";

gamengine.init_window("My Game", 800, 600);

var px = 400;
var py = 300;

loop inf {
    if (gamengine.is_key_pressed("LEFT") == yes)  { px = px - 4; }
    if (gamengine.is_key_pressed("RIGHT") == yes) { px = px + 4; }
    if (gamengine.is_key_pressed("UP") == yes)    { py = py - 4; }
    if (gamengine.is_key_pressed("DOWN") == yes)  { py = py + 4; }

    gamengine.draw_rect(px, py, 32, 32, 0.2, 0.6, 1.0);

    var open = gamengine.update();
    if (open == no) { return; }

    wait(0.016);
}
```

---

## 📖 API Reference

### Window

#### `init_window(title, width, height)`
Creates and shows the game window. Must be called once before any other function.
```
gamengine.init_window("My Game", 800, 600);
gamengine.init_window(800, 600);
```

#### `update()`
Flushes all draw commands to the screen, processes all input events, clears the draw queue and collision boxes for the next frame. Returns `yes` if the window is still open, `no` if it has been closed.
```
var open = gamengine.update();
if (open == no) { return; }
```

---

### 2D Rendering

Colors use **normalized RGB** — values between `0.0` and `1.0`.

#### `draw_rect(x, y, width, height, r, g, b)`
Draws a filled rectangle.
```
gamengine.draw_rect(100, 100, 64, 64, 1.0, 0.0, 0.0);
```

#### `draw_text(text, x, y, r, g, b)`
Draws bold text at the given screen position.
```
gamengine.draw_text("Score: 0", 10, 30, 1.0, 1.0, 1.0);
```

#### `draw_button(text, x, y, width, height, r, g, b, mouse_x, mouse_y, mouse_click)`
Draws an interactive button. Automatically brightens on hover. Auto-picks black or white text based on background brightness. Returns `yes` if the button is clicked this frame.
```
var mx = gamengine.get_mouse_x();
var my = gamengine.get_mouse_y();
var prev_click = no;

loop inf {
    var click_raw = gamengine.is_mouse_pressed("LEFT");
    var click = no;
    if (click_raw == yes && prev_click == no) { click = yes; }
    prev_click = click_raw;

    if (gamengine.draw_button("Start", 300, 250, 200, 55, 0.2, 0.6, 1.0, mx, my, click) == yes) {
        show("Started!");
    }

    gamengine.update();
}
```

> **Tip:** Always use edge detection (`prev_click`) with `draw_button` to avoid a single click registering across multiple frames.

---

### Background & Images

#### `load_background(path, x, y, width, height)`
Loads a JPG or PNG image and draws it at the given position and size every frame, behind all other 2D elements. Only needs to be called once.
```
gamengine.load_background("assets/map.png", 0, 0, 800, 600);
```

---

### 3D Rendering

#### `import_3d(path)`
Loads a 3D model via Assimp. Supports `.obj`, `.fbx`, `.gltf`, `.dae`, and all other formats Assimp supports. Returns the filepath as the model ID. Results are cached — loading the same file twice is free.
```
var ship = gamengine.import_3d("assets/ship.obj");
```

#### `draw_3d(model_id, x, y, z, scale)`
Draws a loaded 3D model at a world position with a given scale. The model auto-rotates on the Y axis every frame.
```
gamengine.draw_3d(ship, 0.0, 0.0, 0.0, 1.0);
```

---

### Audio & Video

#### `play_music(path)`
Plays an audio file via LibVLC. Supports MP3, WAV, FLAC, OGG, and any other format LibVLC supports. Stops any currently playing audio before starting.
```
gamengine.play_music("assets/bgm.mp3");
```

#### `stop_music()`
Stops and releases the currently playing audio.
```
gamengine.stop_music();
```

#### `play_video(path, x, y, width, height)`
Plays a video file directly inside the game window at the given position and size. Frames are decoded into a thread-safe pixel buffer via mutex. Supports any format LibVLC supports.
```
gamengine.play_video("assets/intro.mp4", 200, 100, 400, 300);
```

---

### Keyboard Input

#### `is_key_pressed(key)`
Returns `yes` if the given key is currently held down.

| Key String | Key |
|------------|-----|
| `"UP"` | Arrow Up |
| `"DOWN"` | Arrow Down |
| `"LEFT"` | Arrow Left |
| `"RIGHT"` | Arrow Right |
| `"SPACE"` | Spacebar |
| `"ENTER"` / `"RETURN"` | Enter |
| `"ESC"` / `"ESCAPE"` | Escape |
| `"SHIFT"` | Shift |
| `"CTRL"` / `"CONTROL"` | Control |
| `"ALT"` | Alt |
| `"A"` – `"Z"` | Any single letter key |

```
if (gamengine.is_key_pressed("SPACE") == yes) {
    show("Jump!");
}
```

---

### Mouse Input

#### `is_mouse_pressed(button)`
Returns `yes` if the given mouse button is currently held. Accepts `"LEFT"`, `"RIGHT"`, `"MIDDLE"` / `"CENTER"`.
```
if (gamengine.is_mouse_pressed("LEFT") == yes) { show("Click!"); }
```

#### `get_mouse_x()` / `get_mouse_y()`
Returns the current cursor position in pixels.
```
var mx = gamengine.get_mouse_x();
var my = gamengine.get_mouse_y();
```

#### `get_mouse_scroll()`
Returns the mouse wheel delta for the current frame. Positive = scroll up, negative = scroll down. Resets to `0` after each `update()`.
```
var scroll = gamengine.get_mouse_scroll();
```

---

### Collision Detection

Collision uses **AABB** (Axis-Aligned Bounding Box) rectangle overlap. All walls are automatically cleared after each `update()` call, so walls must be re-registered each frame if they are dynamic.

#### `add_wall(x, y, width, height)`
Registers a blocked rectangle zone for the current frame.
```
gamengine.add_wall(0, 0, 800, 20);
gamengine.add_wall(0, 580, 800, 20);
gamengine.add_wall(0, 0, 20, 600);
gamengine.add_wall(780, 0, 20, 600);
```

#### `check_collision(x, y, width, height)`
Returns `yes` if the given rectangle overlaps any registered wall. Returns `no` if clear.
```
if (gamengine.check_collision(nx, ny, 32, 32) == no) {
    px = nx;
    py = ny;
}
```

---

### Utility

#### `get_random(max)`
Returns a random integer between `0` and `max - 1`.
```
var x = gamengine.get_random(800);
```

---

## 🏗️ Rendering Order Per Frame

Each frame renders in this exact order:

```
1. OpenGL clears the screen
2. OpenGL renders all 3D models
3. QPainter draws the background image (if loaded)
4. QPainter draws the video frame (if playing)
5. QPainter draws all draw_rect, draw_text, draw_button calls
6. update() flushes to screen, clears draw queue and collision boxes
```

3D always sits behind 2D — ideal for HUD overlays, menus, and UI on top of a 3D scene.

---

## ⚙️ Build

**Linux:**
```bash
g++ -o gamengine.so gamengine.cpp -shared -fPIC \
    $(pkg-config --cflags --libs Qt6Widgets Qt6OpenGLWidgets) \
    -lassimp -lvlc -lGL
```

**Windows (MSYS2 / MinGW):**
```bash
g++ -o gamengine.dll gamengine.cpp -shared \
    $(pkg-config --cflags --libs Qt6Widgets Qt6OpenGLWidgets) \
    -lassimp -lvlc -lopengl32
```

**macOS:**
```bash
g++ -o gamengine.dylib gamengine.cpp -shared \
    $(pkg-config --cflags --libs Qt6Widgets Qt6OpenGLWidgets) \
    -lassimp -lvlc -framework OpenGL
```

Place the compiled file inside a `module/` folder next to your `.vl` script.

---

## 💡 Full Example

```
with "module\gamengine.dll";

gamengine.init_window("Dodge!", 800, 600);
gamengine.load_background("assets/bg.png", 0, 0, 800, 600);
gamengine.play_music("assets/bgm.mp3");

var px = 375;
var py = 500;
var lives = 3;
var score = 0;
var prev_click = no;

loop inf {
    var mx = gamengine.get_mouse_x();
    var my = gamengine.get_mouse_y();
    var click_raw = gamengine.is_mouse_pressed("LEFT");
    var click = no;
    if (click_raw == yes && prev_click == no) { click = yes; }
    prev_click = click_raw;

    if (gamengine.is_key_pressed("LEFT") == yes)  { px = px - 5; }
    if (gamengine.is_key_pressed("RIGHT") == yes) { px = px + 5; }
    if (gamengine.is_key_pressed("UP") == yes)    { py = py - 5; }
    if (gamengine.is_key_pressed("DOWN") == yes)  { py = py + 5; }

    gamengine.add_wall(0, 0, 800, 10);
    gamengine.add_wall(0, 590, 800, 10);
    gamengine.add_wall(0, 0, 10, 600);
    gamengine.add_wall(790, 0, 10, 600);

    if (gamengine.check_collision(px, py, 32, 32) == no) {
        gamengine.draw_rect(px, py, 32, 32, 0.2, 0.6, 1.0);
    }

    var hud = "Lives: " + lives + "   Score: " + score;
    gamengine.draw_text(hud, 10, 30, 1.0, 1.0, 1.0);

    if (gamengine.draw_button("Quit", 700, 565, 90, 30, 0.8, 0.1, 0.1, mx, my, click) == yes) {
        gamengine.stop_music();
        return;
    }

    var open = gamengine.update();
    if (open == no) { return; }

    wait(0.016);
}
```

---

## 📋 Function Summary

| Function | Returns | Description |
|----------|---------|-------------|
| `init_window(title, w, h)` | nil | Create and show window |
| `update()` | yes/no | Flush frame, returns window open state |
| `draw_rect(x, y, w, h, r, g, b)` | nil | Draw filled rectangle |
| `draw_text(text, x, y, r, g, b)` | nil | Draw bold text |
| `draw_button(text, x, y, w, h, r, g, b, mx, my, click)` | yes/no | Draw interactive button |
| `load_background(path, x, y, w, h)` | yes/no | Load and set background image |
| `import_3d(path)` | model_id | Load 3D model via Assimp |
| `draw_3d(id, x, y, z, scale)` | nil | Draw 3D model |
| `play_music(path)` | yes/no | Play audio via LibVLC |
| `stop_music()` | yes/no | Stop currently playing audio |
| `play_video(path, x, y, w, h)` | yes/no | Play video inside window |
| `is_key_pressed(key)` | yes/no | Check if key is held |
| `is_mouse_pressed(button)` | yes/no | Check if mouse button is held |
| `get_mouse_x()` | number | Cursor X position |
| `get_mouse_y()` | number | Cursor Y position |
| `get_mouse_scroll()` | number | Mouse wheel delta |
| `add_wall(x, y, w, h)` | nil | Register collision zone |
| `check_collision(x, y, w, h)` | yes/no | Check AABB overlap |
| `get_random(max)` | number | Random integer 0 to max-1 |

---

## 📄 License

```
Copyright 2024 dewieka123

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

---

*Part of the [VerseLanguage](https://github.com/dewieka123/verselanguage) ecosystem — version 2.5-Multiverse*
