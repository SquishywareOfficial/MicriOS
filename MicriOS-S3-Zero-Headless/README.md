# MicriOS for S3-Zero Headless

Headless ESP32-S3-Zero firmware for no-display Micri Deck nodes.

This target is tuned for ESP32-S3-Zero style boards with:

- ESP32-S3 dual-core LX7 at 240 MHz
- 4 MB flash
- 2 MB PSRAM
- USB-C native USB serial
- BOOT button on `GPIO0`
- onboard WS2812 RGB LED on `GPIO21`

Fresh boards default straight into **Slave Miner** so a cluster rack can boot
and start looking for a Distributed Miner master without touching the button.
Hold BOOT for 5 seconds while an app is running to return to the LED menu.

The LED menu still includes two apps:

- **Mouse Emulator**: Bluetooth HID mouse emulator using the Logitech profile.
- **Slave Miner**: Distributed Miner ESP-NOW slave. This is the default launch
  app when no saved autolaunch setting exists.

## Flash

Compile:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=4M,PartitionScheme=huge_app,PSRAM=opi,UploadSpeed=921600 MicriOS-S3-Zero-Headless
```

Upload, replacing `COM11` with the connected port:

```powershell
arduino-cli upload --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=4M,PartitionScheme=huge_app,PSRAM=opi,UploadSpeed=921600 --port COM11 MicriOS-S3-Zero-Headless
```

## LED Menu

The tested ESP32-S3-Zero style board is expected to use a WS2812 RGB LED on
`GPIO21`. MicriOS drives it as a simple blue status LED.

The menu repeats these slots:

| LED menu pattern | Tap window action |
| --- | --- |
| LED on 2 seconds, one short blink, LED off 2 seconds | Tap during the off window to open Mouse Emulator |
| LED on 2 seconds, two short blinks, LED off 2 seconds | Tap during the off window to open Slave Miner |

If you miss the off-window tap, wait for the menu to cycle again.

Double-tap anywhere in the LED menu to clear the saved autolaunch app. On the
S3-Zero build, clearing the saved autolaunch returns the board to the factory
default behavior: booting straight into Slave Miner.

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

Mouse-specific button rules:

| Action | Result |
| --- | --- |
| 1 tap | No action |
| 2 taps | Make Mouse Emulator the saved autolaunch app |
| 3 taps | Clear saved Bluetooth mouse pairing, rotate the BLE address, and return to advertising / pairing mode |
| Hold BOOT for 5 seconds | Exit to LED menu |

## Slave Miner App

Slave Miner waits for a MicriOS Distributed Miner master, pairs over ESP-NOW,
and mines assigned nonce ranges.

This S3-Zero target uses larger slave mining assignments and an ESP32-S3
hardware SHA-256 worker on core 1. By default, core 0 also runs a baked software
SHA helper; it can be disabled at compile time for performance comparisons. The
slave accepts one queued next assignment from the master, which reduces idle
time between batches without requiring constant ESP-NOW chatter.

At the start of the first assignment, the hardware result is checked against
the software implementation. The miner automatically falls back to software if
validation fails. ESP-NOW callbacks only queue packets; the miner task applies
state changes. Assignments and results use application acknowledgements and
bounded retries, and every master run has a new session ID so delayed work from
an earlier run is ignored.

It still receives work over ESP-NOW; it does not need WiFi credentials, wallet
settings, or pool settings.

Slave Miner button rules:

| Action | Result |
| --- | --- |
| 1 tap | No action |
| 2 taps | Make Slave Miner the saved autolaunch app |
| 3 taps | Clear saved master/cluster pairing |
| Hold BOOT for 5 seconds | Exit to LED menu |

Slave Miner LED cycles start with the app marker: LED on, two short blinks, LED
on again, short off gap. Then one state pattern is shown for up to 5 seconds:

| State pattern | Meaning |
| --- | --- |
| Solid on | Actively or recently mining |
| Slow blink | Searching / not paired |
| Very slow blink | Paired but idle |
| Fast blink | Error |

After the state pattern, the app marker repeats and the next state pattern is
shown.

## Pairing Slave Miner

1. Flash and power the S3-Zero.
2. On the T-Display or screened C3 master, open **Distributed Miner**.
3. Start the cluster and open pairing mode.
4. The S3-Zero should pair automatically and begin mining once the master has
   pool work to distribute.

If the S3-Zero was paired to another master or cluster, triple-tap while Slave
Miner is running to clear the saved pairing.

## Serial Output

Serial runs at `115200` baud and prints a status line every 2 seconds.

Miner example:

```text
[headless] build=v3.0 b86 mode=miner state=Mining paired=yes channel=6 khps=450.0 effective_khps=438.2 sha=hardware caps=0xF jobs=42 completed=41 total_kh=123456
```

Serial is useful for confirming pairing, WiFi channel, assignment size, and
whether miner assignments are being completed. `khps` is raw assignment speed;
`effective_khps` includes time between completed results. `sha=hardware`
confirms that the self-test passed. `sha=software-fallback` means the hardware
path failed validation and must be investigated. Rates move in steps rather
than updating continuously during a long batch.

## S3 Performance Comparison

The default build enables the core-0 software helper. To compile a hardware-only
comparison build without editing source:

```powershell
arduino-cli compile --build-property "compiler.cpp.extra_flags=-DMICRI_CLUSTER_S3_SOFTWARE_WORKER=0" --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=4M,PartitionScheme=huge_app,PSRAM=opi,UploadSpeed=921600 MicriOS-S3-Zero-Headless
```

Compare five-minute `khps` and `effective_khps` averages. Keep the software
helper only if it produces a meaningful effective-rate improvement without
causing radio instability, excessive heat, or button/LED starvation.

## Compatibility

The acknowledgement/session changes use cluster protocol version 3. Masters
and slaves must be built from the same b86-era source or newer; older cluster
firmware will intentionally ignore these packets.
