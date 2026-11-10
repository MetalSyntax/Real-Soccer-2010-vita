# Real Soccer 2010 (PS Vita) — Beta Release v1.0

First public release of the native iOS **Mach-O loader** port of *Real Soccer 2010* (Gameloft) for the PlayStation Vita. On real hardware the game boots to the main menu and friendly matches can be played from kick-off, with music, sound effects and physical controls.

---

## 📦 Release Package Contents

1. **`realsoccer10.vpk`** (v01.00): PlayStation Vita application package with the Mach-O loader, Objective-C runtime, vitaGL renderer, OpenAL/AudioQueue audio layer and physical input handling.
2. **`rebase.bin`**: **29,256** rebase words for the 1.1.0 executable (offsets only, no game code).
3. **`binds.bin`**: **1,488** import slots that map the executable's Apple framework imports to the loader.
4. **`nib.txt`**: the window/view layout of `MainWindow.nib`, precompiled (class and outlet names only).
5. **`controls.txt`**: button → touch mapping, editable over FTP.

---

## 🛠️ What is rebase.bin?

`RSoccer10` is a non-PIE armv6 executable linked at a fixed address and without relocations. The loader maps it at `0x98000000`, and `rebase.bin` lists the **29,256** words in its code and data that hold absolute addresses and must be moved to the new base. `binds.bin` lists the **1,488** import slots (417 symbols) to bind. Both are generated from your executable with:

```bash
python3 -I tools/ios_prep.py game_app/RSoccer10 ux0_data/realsoccer10   # run by tools/stage_data.sh
```

They only match **Real Soccer 2010 1.1.0**.

---

## 🚀 Player Installation Guide

1. **PS Vita requirements**:
   - Custom Firmware (**Enso 3.60 / 3.65** recommended).
   - `kubridge.skprx` under `*KERNEL` in `ur0:tai/config.txt`.
   - `libshacccg.suprx` in `ur0:data/` (obtainable via **ShaRKBR33D**).

2. **Game assets**:
   - Extract `Payload/RSoccer10.app/` from your decrypted iOS **1.1.0** `.ipa` (FairPlay `cryptid 0`).
   - Convert its four `.m4a` music tracks to `.ogg` with the same name (otherwise there is no music):
     ```bash
     for f in RSoccer10.app/*.m4a; do ffmpeg -i "$f" -vn -ac 2 -c:a vorbis -strict -2 -q:a 5 "${f%.*}.ogg"; done
     ```
   - Layout:
     ```text
     ux0:data/realsoccer10/
     ├── game_app/      (contents of RSoccer10.app + the .ogg files)
     ├── rebase.bin     (from this release)
     ├── binds.bin      (from this release)
     ├── nib.txt        (from this release)
     ├── controls.txt   (from this release)
     ├── Documents/     (created on first launch, save games)
     └── logs/          (created on first launch, one log per run)
     ```

3. **Install the VPK**: transfer and install `realsoccer10.vpk` with VitaShell.

---

## 🎮 Controls

| PS Vita Input | Action |
| :--- | :--- |
| **Front Touchscreen** | iPhone touch screen (menus, HUD) |
| **Left Stick / D-Pad** | Move the player (virtual pad) and phone tilt |
| **Cross (✖)** | B — shoot |
| **Square (◼)** | A — pass |
| **Circle (●)** | Back |
| **Triangle (▲) / R** | Top-right match buttons |
| **Start** | Tap the screen center |
| **L** | Shake the phone (throw-in) |

Touch points can be edited in `ux0:data/realsoccer10/controls.txt`. Throw-in: hold the left stick fully to one side to aim, then press **L**.

---

## 📋 Changelog (v1.0)

- **Loader**: own Mach-O loader + Objective-C runtime (shared by the iOS ports) → **29,256** rebase words, **1,488** import slots, 0 unimplemented, 8 global constructors.
- **Rebase**: sizes, GL enums and field offsets were rebased as pointers (`bad_alloc` in `new CGlobal` / `new CFMath`, black screen, crash in `InitializeLight`) → `ios_prep.py` classifies literals by use (allocator/GL arguments, register offsets) and handles 12-byte symbol stubs.
- **Display**: picture upside down → `UIDevice orientation` now matches the landscape-right rotation.
- **Audio**: no sound (`alGetError` without a context, untranslated iOS music paths) and no music (`AudioFileGetPropertyInfo` answered for unsupported properties) → fixed; OGG reads use a 64 KB buffer and queued buffers are stopped before unqueueing.
- **Matches**: crash in `CAnimationManager::ReleaseMeshAndTex` (uninitialized fields) → `operator new` returns zeroed memory as on iOS.
- **Matches**: main thread froze right after kick-off → `GlRenderer::Init` overflowed vitaGL's 32-level modelview stack; the loader keeps its own 128-level matrix stacks.
- **Matches**: crash ~30 s in (`CFormationPool::Update`) → ball trajectory overflowed its 300-frame array in `CFootBall::SetBallOutTo_Reflect`; patched to stop at 299 frames.
- **Graphics**: flashes with vitaGL's native `GL_FIXED` and `DRAW_SPEEDHACK` → `GL_FIXED` converted on CPU, only for the drawn range and enabled arrays; speedhack removed.
- **Graphics**: one-frame flashes from fixed-point skinning overflow in player/shadow matrices → 3D draws whose modelview scale exceeds 16.0 are dropped.
- **Graphics**: texture env mode is now tracked per texture unit (multitexture pitch).
- **Controls**: virtual pad on the left stick (`STICK`), up to 9 touches, and `SHAKE` (accelerometer jolt) for throw-ins.
- **Menus**: "More games" (IGP) disabled — it switched the screen to portrait.

---

## ⚠️ Known Issues

- The one-frame flash fix and the throw-in (`SHAKE`) control are in this build but have not been confirmed on console yet.
- Some match scenes drop to 10–15 fps.
- vitaGL does not implement `GL_SPOT_*`; spotlight parameters are ignored.
- Intro video is skipped.
- GameKit multiplayer, "More games" and Gameloft Live are disabled.
- No trophies yet.

Installation and build instructions: see [README.md](README.md).
