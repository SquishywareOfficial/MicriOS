# MicriOS For ESP32-CYD

The `MicriOS-CYD` target supports the common `ESP32-2432S028` Cheap Yellow
Display electrical profile: a classic dual-core ESP32, `320x240` ILI9341 TFT,
and XPT2046 resistive touch controller.

## First Boot

MicriOS starts a four-point touch calibration the first time it boots. Touch
and release each yellow target with the stylus or a firm fingertip. The result
is stored in ESP32 Preferences and reused after a restart or firmware update.
Firmware also invalidates calibration when the display orientation layout
changes, so the first boot after an orientation update may request calibration
again.

To recalibrate later, open **Settings / Device / Calibrate touch**. Holding the
board's BOOT button while resetting also forces calibration.

## Touch Controls

- The root screen is a `2x2` icon-and-label card dashboard.
- Games, Apps, and Settings use paged `2x3` icon-and-label card grids. Every
  launcher entry has a distinct code-drawn icon based on its splash art or
  purpose, so the menu does not depend on text alone.
- Tap a card to open it.
- Swipe horizontally or tap the page arrows in the menu header to change
  pages. Paging controls stay in the menu canvas rather than consuming system
  bar space.
- Page changes are snap transitions: MicriOS draws the complete next page into
  the off-screen framebuffer and pushes it in one operation. There is no
  animated or inertial scrolling that can stutter on the ESP32.
- Tap **Exit** in the bottom system bar to leave an app. The same control says
  **Back** while navigating a submenu.
- Apps use their visible cards, rows, tabs, sliders, and action buttons directly.
  Touching unused app space does not fall through to hidden button controls.
- One-input games use the entire game canvas as the primary action only while
  gameplay is running. Intro and result screens use visible Start/Play Again
  buttons.
- Two-direction games use the left and right halves of the game canvas while
  gameplay is running.

The full MicriOS T-Display app/game suite is registered in the CYD build. The
shell and utility interfaces are direct-touch. Maze intersections expose their
valid directions, Pipe Mania accepts its highlighted cell, Pet Simulator uses
care-action cards, and Micri Casino/Tong-its expose their cards, piles, bets,
melds, and table actions. One-action and direction games retain full-canvas
controls because those interactions suit the games rather than the old button
limitations.

Destructive settings use explicit Cancel and Delete/Reset dialogs. Save
Manager's Delete All flow retains two separate confirmation dialogs.

The bottom system bar is not part of the game input area, so exiting cannot
also trigger an in-game action. Apps may only clear the `320x208` content
canvas; the shell owns the final 32 pixels. Screen savers are the exception:
they render immersively across `320x240` with no permanent bar. Tap the lower
edge to reveal a temporary **Exit** control.

The system bar also contains brightness, volume, and battery icons. Tap the
sun to open a brightness slider entirely inside the 32-pixel bar; tap or drag
the slider to preview the backlight level. The setting is persisted when the
touch is released. The overlay is drawn once and subsequent drag samples
redraw only the slider track to avoid full-width flashing. Volume and battery
use the same shared control interface,
but the current CYD hardware implementation shows `Not implemented` until
speaker and battery-sensing hardware are added. The bar is redrawn only when
its state changes, not on every animated app frame.

## Device Settings

The CYD Device settings page includes:

- `1..16` backlight brightness, persisted across restarts.
- Touch recalibration.
- Live raw/calibrated touch diagnostics.
- Active-low onboard RGB LED test hooks.

Games that used the C3 status LED use the CYD RGB LED for equivalent cues:
landing burn timing, shift timing, fishing-line tension, and Micri Field timing.
The RGB LED is cleared whenever one of those games exits.

WiFi Setup, Communicator, ESP Contacts, Mouse Emulator, Micri Miner, and
Distributed Miner retain their full radio functionality. Their target app exit
hooks stop AP/station, ESP-NOW, BLE, socket, and miner resources before the
launcher resumes.

GPIO `22` and `27` are reserved as likely future physical game-button inputs.
They are disabled by default and feed the same logical B1/B2 router when
enabled at compile time.

## Local Build And Upload

From the repository root:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app MicriOS-CYD
arduino-cli upload --fqbn esp32:esp32:esp32:PartitionScheme=huge_app --port COM20 MicriOS-CYD
```

Replace `COM20` with the port shown by `arduino-cli board list`.

## Hardware Pin Profile

| Function | GPIO |
| --- | ---: |
| TFT DC | 2 |
| TFT MISO | 12 |
| TFT MOSI | 13 |
| TFT CLK | 14 |
| TFT CS | 15 |
| TFT backlight | 21 |
| Touch CLK | 25 |
| Touch MOSI | 32 |
| Touch CS | 33 |
| Touch IRQ | 36 |
| Touch MISO | 39 |
| RGB red / green / blue | 4 / 16 / 17 |

Cheap Yellow Display variants exist with different screens and wiring. This
target is specifically for the PCB marked `ESP32-2432S028` using the profile
above.

The ESP32-2432S028R requires TFT_eSPI's alternate `ILI9341_2_DRIVER`
initialization. The firmware then prefers rotation `1`, verifies that the
driver reports a `320x240` landscape canvas, and falls back through the other
rotations for compatible panel variants. All CYD framebuffer pushes reapply
that validated orientation before drawing.

`CydHardware` exclusively owns backlight GPIO21. It attaches LEDC PWM after
`TFT_eSPI::init()`; defining `TFT_BL` in the TFT setup allows the display
library to reclaim the pin and makes brightness adjustment appear ineffective.
The tested ESP32-2432S028R panel requires controller inversion enabled for
requested `TFT_BLACK` pixels to appear black. The target therefore uses
`TFT_INVERSION_ON` and explicitly calls `invertDisplay(true)` after init.
