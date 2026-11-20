<img width="1015" height="300" alt="1000954317" src="https://github.com/user-attachments/assets/a8de0a35-974d-4d0b-8f3e-f02033b0edaf" />

# DSDA DOOM for Android

An Android port of [DSDA-Doom](https://github.com/kraflab/dsda-doom) — an advanced Doom source port (successor of PrBoom+) built for demo recording, speedrunning and accurate vanilla/Boom/MBF/MBF21 compatibility — with a simple launcher and touch controls.

- Engine: DSDA-Doom (software renderer) on top of SDL2 / SDL2_mixer
- Launcher: pick the game, mods, difficulty and compatibility level, then play
- Demos: record every run and watch recorded demos from inside the app
- Touch UI: on-screen stick, fire/use/weapon buttons — fully movable and resizable

> The app does **not** include any commercial game data. You need your own IWAD (`DOOM.WAD`, `DOOM2.WAD`, `PLUTONIA.WAD`, `TNT.WAD`, Freedoom, etc.).

## Features

- Launcher with IWAD / PWAD selection (`.wad`, `.pk3`, `.pk7`, `.zip`, `.deh`, `.bex`)
- Skill levels 1–5 and warp to a given map (`E1M1` / `MAP01` style detected automatically)
- Compatibility levels: Doom v1.9, Ultimate Doom, Final Doom, Boom, MBF, MBF21 (suggested automatically from the selected WAD)
- Render resolution from 360p to 1080p
- Demo recording (on by default) and a demo picker for playback
- Extra command-line arguments field for any DSDA-Doom option
- On-screen controls with an "Edit Touch" mode to drag and resize each button
- Adjustable look sensitivity, gamepad and keyboard supported through SDL2
- All launcher settings are remembered between sessions

## Installation

1. Build the APK (see below) and install it, or install a prebuilt one.
2. Open the app and tap **Grant storage access** (recommended — it lets the app use the shared folder below).
3. Copy your IWAD (and any mods) into the `WADS` folder.
4. Pick a game, press **PLAY**.

### Folder layout

With storage access granted, everything lives in `DSDA DEMO` on the shared storage:

```
DSDA DEMO/
├── WADS/    # put IWADs and PWADs here
├── data/    # config, saves, dsda-doom.wad (engine data, copied automatically)
└── *.lmp    # recorded demos
```

Without storage access the app falls back to its own app-specific folder (`Android/data/com.ds.da/files`); the launcher shows the exact path in use.

## Controls (landscape)

| Action | Gesture |
|---|---|
| Move | Touch the left side — an analog stick appears under your thumb |
| Look | Drag anywhere else (also works while holding Fire) |
| Fire | Big disc (hold) |
| Use | Small ring |
| Next / previous weapon | `>` / `<` |
| Menu | Three bars, top right |
| Automap | Small square, top |

In menus there is no D-pad: tap an item to activate it and drag up/down to scroll. Y/N buttons appear when the game asks a question, and the keyboard opens automatically when text input is needed.

## Building

Requirements: Android Studio (or just the Android SDK), JDK 11+, Android NDK `29.0.14033849`, CMake `3.31.6`. An internet connection is needed on the first build.

```bash
./gradlew assembleRelease     # or assembleDebug
```

The first Gradle run downloads SDL2 2.32.10, SDL2_mixer 2.8.1 and libzip 1.11.4 into `app/src/main/cpp/third_party/` (task `downloadDeps`, runs automatically).

Target: `arm64-v8a`, `minSdk 21`, `targetSdk 34`.

## Project structure

```
app/src/main/
├── java/com/ds/da/
│   ├── LauncherActivity.java   # launcher UI, WAD scanning, builds engine arguments
│   ├── GameActivity.java       # SDLActivity host: immersive mode, env setup
│   ├── TouchEditActivity.java  # on-screen button editor
│   └── TouchLayout.java        # saved button layout
├── cpp/
│   ├── dsda-doom/              # DSDA-Doom engine sources
│   ├── android/                # Android glue: touch controls, paths, GL stubs, sound loader
│   ├── third_party/            # SDL2, SDL2_mixer, libzip (downloaded at build time)
│   ├── CMakeLists.txt
│   └── sources.cmake           # engine source list
├── assets/dsda-doom.wad        # engine data WAD
└── res/                        # icons, theme, strings
```

## Credits and license

- [DSDA-Doom](https://github.com/kraflab/dsda-doom) by Kraflab and contributors, based on PrBoom+ / Boom / Doom by id Software
- [SDL2](https://libsdl.org), SDL2_mixer, [libzip](https://libzip.org)

The engine is distributed under the **GNU GPL v2** (see `app/src/main/cpp/dsda-doom/COPYING`). Doom and its game data are property of their respective owners and are not included.
