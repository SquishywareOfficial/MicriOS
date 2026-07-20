# Flashing MicriOS

The easiest way to install MicriOS and turn a supported ESP32 board into a Micri Deck is the browser flasher:

[https://squishywareofficial.github.io/MicriOS/](https://squishywareofficial.github.io/MicriOS/)

Use Chrome or Edge on a desktop computer. Mobile browsers usually do not expose USB serial flashing.

## Which Build To Pick

- **MicriOS for C3**: ESP32-C3 with the 0.42 inch `72x40` OLED.
- **MicriOS for T-Display**: classic ESP32 T-Display with `240x135` color TFT.
- **MicriOS for CYD**: ESP32-2432S028 with `320x240` ILI9341 touch TFT.
- **MicriOS for C3 Headless**: no-screen ESP32-C3 with LED/button launcher, Mouse Emulator, and Distributed Miner slave.
- **MicriOS for S3-Zero Headless**: dedicated no-screen ESP32-S3-Zero Distributed Miner slave with RGB status LED.

## Browser Flashing

1. Connect the board over USB.
2. Open the [web flasher](https://squishywareofficial.github.io/MicriOS/).
3. Click **Install** on the card that matches your board.
4. Choose the board serial port when the browser prompts for it.
5. Wait for flashing to complete, then reset or reconnect the board if needed.

The installer page has separate install buttons and manifests for each firmware target. The browser only asks for the USB serial port after you choose the board card.

If the board is not detected or flashing fails, hold **BOOT** while plugging the board in, then try again.

## Releases Versus Web Flasher

The web flasher and GitHub Releases are related but separate:

- **Web flasher**: uses firmware from the latest successful GitHub Pages deployment from `main`.
- **GitHub Releases**: versioned ZIP files attached to tags such as `v2.0-b77`.

For most users, the web flasher is simpler. Releases are useful when you want a specific version, offline copy, or manual `esptool` flashing.

## Manual Flashing From A Release

Download a firmware ZIP from the [latest release](https://github.com/SquishywareOfficial/MicriOS/releases/latest), extract it, and use the merged binary.

Windows release ZIPs include a helper:

```bat
flash-windows.bat COM9
```

Replace `COM9` with your board's actual serial port.

Manual `esptool` commands:

```powershell
python -m esptool --chip esp32c3 --port COM9 --baud 460800 write_flash -z 0x0 MicriOS-C3-merged.bin
```

```powershell
python -m esptool --chip esp32c3 --port COM9 --baud 460800 write_flash -z 0x0 MicriOS-C3-Headless-merged.bin
```

```powershell
python -m esptool --chip esp32s3 --port COM11 --baud 460800 write_flash -z 0x0 MicriOS-S3-Zero-Headless-merged.bin
```

```powershell
python -m esptool --chip esp32 --port COM6 --baud 460800 write_flash -z 0x0 MicriOS-T-Display-merged.bin
```

```powershell
python -m esptool --chip esp32 --port COM20 --baud 460800 write_flash -z 0x0 MicriOS-CYD-merged.bin
```

On Linux/macOS, use the same commands with `python3` and a serial device such as `/dev/ttyACM0` or `/dev/ttyUSB0`.

## Manual Flashing After A Local Build

Run these commands from the repository root.

First build everything locally. This also installs the Arduino ESP32 core, required libraries, and syncs the root `shared/` folder into each sketch before compiling:

Windows:

```powershell
.\build.ps1
```

Linux/macOS:

```sh
./build.sh
```

Then find the board serial port:

```powershell
arduino-cli board list
```

Upload the matching sketch with `arduino-cli`:

```powershell
arduino-cli upload --fqbn esp32:esp32:esp32c3:PartitionScheme=huge_app --port COM9 MicriOS-C3
```

```powershell
arduino-cli upload --fqbn esp32:esp32:esp32c3:PartitionScheme=huge_app --port COM9 MicriOS-C3-Headless
```

```powershell
arduino-cli upload --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=4M,PartitionScheme=huge_app,PSRAM=enabled,UploadSpeed=921600 --port COM11 MicriOS-S3-Zero-Headless
```

```powershell
arduino-cli upload --fqbn esp32:esp32:esp32:PartitionScheme=huge_app --port COM6 MicriOS-T-Display
```

```powershell
arduino-cli upload --fqbn esp32:esp32:esp32:PartitionScheme=huge_app --port COM20 MicriOS-CYD
```

Replace `COM9` / `COM6` with the port from `arduino-cli board list`. On Linux/macOS, use a serial device such as `/dev/ttyACM0` or `/dev/ttyUSB0`.

### Optional: Create A Local Merged Binary

The normal local build scripts compile the sketches, but release-style merged binaries are created by compiling with `--export-binaries`.

Examples:

```powershell
arduino-cli compile --export-binaries --fqbn esp32:esp32:esp32c3:PartitionScheme=huge_app MicriOS-C3
```

```powershell
arduino-cli compile --export-binaries --fqbn esp32:esp32:esp32c3:PartitionScheme=huge_app MicriOS-C3-Headless
```

```powershell
arduino-cli compile --export-binaries --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=4M,PartitionScheme=huge_app,PSRAM=enabled,UploadSpeed=921600 MicriOS-S3-Zero-Headless
```

```powershell
arduino-cli compile --export-binaries --fqbn esp32:esp32:esp32:PartitionScheme=huge_app MicriOS-T-Display
```

```powershell
arduino-cli compile --export-binaries --fqbn esp32:esp32:esp32:PartitionScheme=huge_app MicriOS-CYD
```

Find the generated merged binary:

```powershell
Get-ChildItem . -Recurse -Filter *.merged.bin
```

Then flash it with `esptool`, using the correct chip family:

```powershell
python -m esptool --chip esp32c3 --port COM9 --baud 460800 write_flash -z 0x0 .\MicriOS-C3\build\...\MicriOS-C3.ino.merged.bin
```

```powershell
python -m esptool --chip esp32c3 --port COM9 --baud 460800 write_flash -z 0x0 .\MicriOS-C3-Headless\build\...\MicriOS-C3-Headless.ino.merged.bin
```

```powershell
python -m esptool --chip esp32s3 --port COM11 --baud 460800 write_flash -z 0x0 .\MicriOS-S3-Zero-Headless\build\...\MicriOS-S3-Zero-Headless.ino.merged.bin
```

```powershell
python -m esptool --chip esp32 --port COM6 --baud 460800 write_flash -z 0x0 .\MicriOS-T-Display\build\...\MicriOS-T-Display.ino.merged.bin
```

```powershell
python -m esptool --chip esp32 --port COM20 --baud 460800 write_flash -z 0x0 .\MicriOS-CYD\build\...\MicriOS-CYD.ino.merged.bin
```

The `...\` in those paths is a placeholder for the board build folder printed by `Get-ChildItem`; use the actual merged binary path on your machine.

## Finding The Serial Port

On Windows, common options:

- Arduino IDE: **Tools / Port**.
- Device Manager: **Ports (COM & LPT)**.
- PowerShell:

```powershell
[System.IO.Ports.SerialPort]::getportnames()
```

If multiple ports are shown, unplug the board, run the command, plug it back in, and run it again. The new port is usually the board.

## Troubleshooting

- Hold **BOOT** while connecting USB if flashing cannot enter bootloader mode.
- Try a different USB cable. Charge-only cables are very common.
- Close Arduino Serial Monitor or any other program using the COM port.
- For ESP32-C3 boards with a tiny OLED, make sure you selected **MicriOS for C3**, not the T-Display build.
- For T-Display boards, make sure you selected **MicriOS for T-Display**, not an ESP32-C3 build.
- For ESP32-S3-Zero boards, make sure you selected **MicriOS for S3-Zero Headless**, not the C3 headless build.
