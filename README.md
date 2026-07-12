# MicriOS

MicriOS is a small multi-app handheld OS for tiny ESP32 boards. It started as an expansion of Atomic14's [esp32-c3-oled-single-button-games](https://github.com/atomic14/esp32-c3-oled-single-button-games), then grew into a ground-up rewrite with games, utilities, WiFi tools, ESP-NOW messaging, solo mining, and distributed miner modes.

Quick links:

- [Web flasher](https://squishywareofficial.github.io/MicriOS/)
- [C3 simulator](https://squishywareofficial.github.io/MicriOS/simulator/)
- [Latest release](https://github.com/SquishywareOfficial/MicriOS/releases/latest)
- [Release notes](RELEASE_NOTES.md)
- [Documentation index](docs/README.md)
- [Flashing guide](docs/flashing.md)
- [Game guides](docs/games.md)
- [Mouse Emulator (Mouse Jiggler) guide](docs/mouse-emulator.md)
- [Micri Miner guide](docs/micri-miner.md)
- [Distributed Miner guide](docs/distributed-miner.md)

## Getting Started

If you already have a supported ESP32 board and just want to put MicriOS on it:

1. Open the [web flasher](https://squishywareofficial.github.io/MicriOS/) in Chrome or Edge.
2. Connect the board over USB.
3. Click **Install** on the card that matches your board:
   - **MicriOS for C3** for the ESP32-C3 0.42 inch OLED board.
   - **MicriOS for T-Display** for the TENSTAR/LilyGO-style ESP32 T-Display.
   - **MicriOS for C3 Headless** for no-screen ESP32-C3 boards used as Distributed Miner slaves.
4. Choose the serial port when the browser prompts for it.
5. If flashing fails, hold **BOOT** while plugging in the board, then try again.

The web flasher uses the latest successful GitHub Pages deployment from `main`. Tagged [GitHub Releases](https://github.com/SquishywareOfficial/MicriOS/releases) are downloadable snapshots with ZIP files for manual flashing.

For fallback flashing commands, browser requirements, and release asset details, see the [flashing guide](docs/flashing.md).

## Supported Hardware

### ESP32-C3 0.42 Inch OLED

- ESP32-C3 module with a `72x40` monochrome OLED.
- OLED on I2C pins `GPIO5` / `GPIO6`.
- BOOT button on `GPIO9`.
- Optional controllable LED on `GPIO8`.

### ESP32-C3 Headless

- Generic ESP32-C3 board with no display.
- BOOT button on `GPIO9`.
- Status LED on `GPIO8`.
- LED/button launcher with Mouse Emulator and Distributed Miner slave apps.
- Fresh boards start in the LED menu unless an app has been saved for autolaunch; double-tap in the LED menu clears autolaunch.

### TENSTAR / LilyGO-Style T-Display ESP32

- Classic ESP32 with `240x135` ST7789 color TFT.
- Buttons on `GPIO0` and `GPIO35`.
- Backlight on `GPIO4`.

## Controls

### MicriOS for C3

- Menu tap: next item.
- Menu hold: open selected item.
- App/game tap: primary action.
- App/game hold: secondary action or return.

### MicriOS for T-Display

- Button 1 tap: next / primary action.
- Button 1 hold: launch / secondary action.
- Button 2 tap: previous / back.
- Button 2 hold: global exit to menu.

## Included Features

MicriOS includes arcade-style games, small utilities, settings tools, ESP-NOW utilities, and miner tools. Most mature apps keep logic in `shared/` and use board-specific rendering/input code for the tiny OLED or larger T-Display.

Radio and network utilities:

- **WiFi Setup** saves up to five WiFi networks through a captive portal.
- **Mouse Emulator (Mouse Jiggler)** advertises as a selectable Bluetooth HID mouse profile and performs occasional humanized movement sweeps. See the [Mouse Emulator guide](docs/mouse-emulator.md).
- **ESP Contacts** exchanges saved initials over ESP-NOW and stores peer MAC addresses.
- **Communicator** sends predefined ESP-NOW messages to **ALL** or a saved contact.
- **Micri Miner** is a solo Stratum miner with wallet/pool setup. See the [Micri Miner guide](docs/micri-miner.md).
- **Distributed Miner** lets one screened board coordinate nearby ESP-NOW mining slaves. See the [Distributed Miner guide](docs/distributed-miner.md).

Device settings and save data can be managed from **Options / Save Manager**. Save Manager can clear individual app data such as scores, contacts, WiFi profiles, miner settings, miner lifetime stats, and cluster pairing data.

## Building From Source

Use the build script for your platform. It installs or updates the ESP32 Arduino core, installs required libraries, copies the root `shared/` source into each sketch's generated `src/shared/` folder, and compiles all three firmware targets.

Windows:

```powershell
.\build.ps1
```

Linux/macOS:

```sh
./build.sh
```

The root `shared/` folder is the single tracked source of truth for shared logic. Board-local `src/shared/` folders are generated during local and GitHub Actions builds and are ignored by Git.

The build scripts do not bump `Version.h`. Release/version bumps should be made explicitly.

## Project Layout

- `MicriOS-C3/`: ESP32-C3 OLED sketch.
- `MicriOS-T-Display/`: T-Display sketch.
- `MicriOS-C3-Headless/`: no-display ESP32-C3 sketch.
- `shared/`: source of truth for shared logic copied into each sketch before build.
- `sim/`: browser-based C3 simulator and editors.
- `docs/`: focused documentation for flashing and larger app workflows.
- `licenses/`: third-party license notices.

## Browser Installer And Simulator

- Web flasher: [https://squishywareofficial.github.io/MicriOS/](https://squishywareofficial.github.io/MicriOS/)
- Simulator: [https://squishywareofficial.github.io/MicriOS/simulator/](https://squishywareofficial.github.io/MicriOS/simulator/)

The flasher currently supports the C3 OLED, T-Display, and headless C3 builds.

## License

MicriOS is released under WTFPL + No Warranty Disclaimer. See [LICENSE](LICENSE).

Micri Miner includes MIT-attributed mining/Stratum logic adapted from NerdMiner_v2. See [licenses/NerdMiner_v2-MIT.txt](licenses/NerdMiner_v2-MIT.txt).
