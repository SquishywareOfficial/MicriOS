# MicriOS for C3 Headless

Headless ESP32-C3 firmware for no-display Micri Deck nodes.

This sketch is for ESP32-C3 SuperMini style boards without a screen. It now has a tiny LED/button launcher with two apps:

- **Mouse Emulator**: Bluetooth HID mouse emulator using the Logitech profile.
- **Slave Miner**: Distributed Miner ESP-NOW slave.

Fresh boards start in the LED menu until an app is saved for autolaunch. Double-tap while an app is running to make that app the saved autolaunch app. Double-tap in the LED menu to clear the saved autolaunch app and make the menu the boot default again. Hold BOOT for 5 seconds while an app is running to exit to the LED menu.

## Flash

Compile:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32c3:PartitionScheme=huge_app MicriOS-C3-Headless
```

Upload, replacing `COM11` with the connected port:

```powershell
arduino-cli upload --fqbn esp32:esp32:esp32c3:PartitionScheme=huge_app --port COM11 MicriOS-C3-Headless
```

## LED Menu

The tested ESP32-C3 SuperMini board uses GPIO8 for the blue LED, and the LED is active-low.

The menu repeats these slots:

| LED menu pattern | Tap window action |
| --- | --- |
| LED on 2 seconds, one short blink, LED off 2 seconds | Tap during the off window to open Mouse Emulator |
| LED on 2 seconds, two short blinks, LED off 2 seconds | Tap during the off window to open Slave Miner |

If you miss the off-window tap, wait for the menu to cycle again.

Double-tap anywhere in the LED menu to clear the saved autolaunch app. The LED gives three quick confirmation blinks, then resumes the menu.

## Shared App Button Rules

| Action | Result |
| --- | --- |
| 1 tap | No action, to avoid accidental taps |
| 2 taps | Make the current app the saved autolaunch app |
| Hold BOOT for 5 seconds | Exit the current app and return to the LED menu |

## Mouse Emulator App

The headless Mouse Emulator always advertises as:

```text
Logitech Signature M650
```

It uses the same movement behavior as the screened Mouse Emulator:

- random movement interval: 10-50 seconds
- movement length: 500-5000 pixels
- small vertical wiggle while moving
- forward movement, short pause, then return movement

Mouse-specific button rules:

| Action | Result |
| --- | --- |
| 1 tap | No action |
| 2 taps | Make Mouse Emulator the saved autolaunch app |
| 3 taps | Clear saved Bluetooth mouse pairing, rotate the BLE address, and return to advertising / pairing mode |
| Hold BOOT for 5 seconds | Exit to LED menu |

Mouse LED cycles start with the app marker: LED on, one short blink, LED on again, short off gap. Then one state pattern is shown:

| State pattern | Meaning |
| --- | --- |
| Solid on for 5 seconds | Connected to computer and active |
| Solid on 5 seconds, then one blink | Connected to computer but paused |
| Solid on 5 seconds, then two blinks | Not connected / Bluetooth unavailable |
| Solid on 5 seconds, then three blinks | Advertising / pairing mode |

After the state pattern, the app marker repeats and the next state pattern is shown.

If the host still shows an old paired mouse entry after a triple-tap reset, remove that old Bluetooth device on the host and pair again. The reset rotates the ESP32-side BLE address to prevent immediate automatic reconnects from stale host-side pairing data.

## Slave Miner App

Slave Miner waits for a MicriOS Distributed Miner master, pairs over ESP-NOW, and mines assigned nonce ranges.

Slave Miner button rules:

| Action | Result |
| --- | --- |
| 1 tap | No action |
| 2 taps | Make Slave Miner the saved autolaunch app |
| 3 taps | Clear saved master/cluster pairing |
| Hold BOOT for 5 seconds | Exit to LED menu |

Slave Miner LED cycles start with the app marker: LED on, two short blinks, LED on again, short off gap. Then one state pattern is shown for up to 5 seconds:

| State pattern | Meaning |
| --- | --- |
| Solid on | Actively or recently mining |
| Slow blink | Searching / not paired |
| Very slow blink | Paired but idle |
| Fast blink | Error |

After the state pattern, the app marker repeats and the next state pattern is shown.

## Pairing Slave Miner

1. Flash and power the headless C3.
2. If needed, hold BOOT for 5 seconds to exit to the LED menu and select Slave Miner.
3. On the T-Display or screened C3 master, open **Distributed Miner**.
4. Start the cluster and open pairing mode.
5. The headless C3 should pair automatically and begin mining once the master has pool work to distribute.

If the C3 was paired to another master or cluster, triple-tap while Slave Miner is running to clear the saved pairing.

## Serial Output

Serial runs at `115200` baud and prints a status line every 2 seconds.

Mouse example:

```text
[headless] build=v2.0 b77 mode=mouse connected=no enabled=yes moving=no
```

Miner example:

```text
[headless] build=v2.0 b77 mode=miner state=Mining paired=yes channel=6 khps=1.1 jobs=42 completed=41 total_kh=1234
```

Menu example:

```text
[headless] build=v2.0 b77 mode=menu slot=mouse
```

Serial is useful for confirming the active app, saved pairing, WiFi channel, and whether miner assignments are being completed.
