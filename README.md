# CFS23 Trainer

![version](https://img.shields.io/badge/version-v0.1-blue)
![platform](https://img.shields.io/badge/platform-Windows%20x64-0078D6)
![engine](https://img.shields.io/badge/engine-Unity%20IL2CPP%20v39-orange)
![license](https://img.shields.io/badge/license-MIT-green)

A small trainer for **Car For Sale Simulator 2023**.  
Built on the IL2CPP API + MinHook, with a GDI overlay menu and car ESP.

> ⚠️ **Single-player only.** This project contains no anti-cheat bypass, does not work with multiplayer, and includes no game files, metadata, or dumps.

---

## Features

| Menu option | Description |
|---|---|
| Infinite money | Sets `CashManager.cash = 999,999,999`. Works even in the main menu |
| BHop / auto jump | Automatic jumping for the on-foot player |
| Surf boost | Speed boost while sliding |
| Auto strafe | Rotates view/yaw toward movement velocity |
| Foot speed | Walk speed multiplier: x1.8 / x3 / x6 / x12 |
| Car speed | Car velocity multiplier: x2 / x5 / x10 / x25 / x50 |
| Fly / noclip | Flight mode: WASD + Space / Ctrl |
| ESP cars | Boxes and distance labels for vehicles |
| No fog | Disables fog |
| FOV | 80 / 95 / 110 / 130 |
| Time scale | 1x / 0.5x / 2x / 0.1x |

---

## Quick start

1. Download `CarDealerCheat.dll` and `Injector.exe` from **Releases → v0.1**.
2. Start the game and wait until it is fully loaded.
3. Run `Injector.exe`.
4. Press **INSERT** in-game to open the menu.
5. Use **↑ / ↓** to select an option, **Enter** to toggle it.

A log file is created next to the game executable: `CFS23_cheat.log`.

---

## Building from source

Requires Visual Studio with C++17 support and PowerShell.

1. Open `build.bat`.
2. If needed, adjust the `VCVARS` path for your Visual Studio installation.
3. Run `build.bat`.

The output will be in `bin\`:

```text
bin\CarDealerCheat.dll
bin\Injector.exe
```

---

## offsets.txt

`offsets.txt` contains  four RVAs relative to `GameAssembly.dll`:

```text
# FirstPersonMovement.Update
674190
# FirstPersonMovement.FixedUpdate
673630
# CarController.FixedUpdate
7871C0
# CarController.Update
7897F0
```

Lines starting with `#` are ignored. If the game updates and the hooks stop working, adjust these values.

---

## Repository layout

```text
src/Cheat.cpp              — DLL entry, reflection, hooks, overlay
src/Injector.cpp           — injector via CreateRemoteThread/LoadLibraryA
include/MinHook.h          — MinHook v1.3.4
lib/libMinHook.x64.lib     — MinHook v1.3.4
build.bat                  — MSVC build script
offsets.txt                — RVAs for the supported game build
CHANGELOG.md               — version history
THIRD_PARTY_NOTICES.md     — MinHook license
bin/                       — ready binaries for Releases (gitignored)
```

---

## Notes

- Money is applied from a background thread via a static field, so it also works in the main menu.
- Other features are applied inside gameplay hooks, so they take effect in-game.
- The overlay is a separate GDI window with `WS_EX_LAYERED | WS_EX_TRANSPARENT` and does not capture input.
- After restarting the game, inject the DLL again.

---

## License

This project is released under the [MIT License](LICENSE).  
The bundled MinHook is also MIT-licensed — see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Use only with your own copy of the single-player game.
