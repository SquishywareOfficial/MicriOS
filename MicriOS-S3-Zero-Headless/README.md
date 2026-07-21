# MicriOS for S3-Zero Headless

Dedicated Distributed Miner slave firmware for no-display ESP32-S3-Zero nodes.
It boots directly into the slave engine and contains no app launcher or Bluetooth
Mouse Emulator.

The target is configured for ESP32-S3FH4R2-style boards with:

- dual-core ESP32-S3 LX7 at 240 MHz
- 4 MB Quad-SPI flash
- 2 MB Quad-SPI PSRAM
- BOOT button on `GPIO0`
- onboard WS2812 RGB LED on `GPIO21`

The miner receives work over ESP-NOW. It does not need WiFi credentials, wallet
settings, or pool settings; those belong to the screened Distributed Miner
master.

## Build And Upload

Run from the repository root.

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=4M,PartitionScheme=huge_app,PSRAM=enabled,UploadSpeed=921600 MicriOS-S3-Zero-Headless
```

Replace `COM11` with the connected port:

```powershell
arduino-cli upload --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=4M,PartitionScheme=huge_app,PSRAM=enabled,UploadSpeed=921600 --port COM11 MicriOS-S3-Zero-Headless
```

`PSRAM=enabled` selects QSPI PSRAM. Do not use `PSRAM=opi` for the
ESP32-S3FH4R2 package.

## RGB Status

The LED uses restrained brightness so six nearby nodes remain readable without
being dazzling.

| LED | Meaning |
| --- | --- |
| Brief white at boot | Firmware and LED self-test |
| Slow red pulse | No saved master; searching for pairing beacons |
| Slow amber pulse | A master is saved but unavailable/stale; reconnecting |
| Solid blue | Paired and awaiting work |
| Solid green | Actively or recently mining |
| Fast red flash | Miner, radio, task, or SHA validation error |
| Brief purple | Slave engine restarted using BOOT |
| Three cyan flashes | Saved master/cluster pairing cleared |

Green remains visible briefly between assignments so normal assignment turnover
does not make the LED chatter between green and blue. Errors override temporary
acknowledgement patterns. A single BOOT tap toggles all informational LED output.
The choice is saved, so a silenced node remains dark after power cycling. Serial
diagnostics continue normally while the LED is disabled.

## BOOT Controls

| Action | Result |
| --- | --- |
| 1 tap | Enable or disable all informational LED output; saved across restarts |
| 2 taps | No action |
| 3 taps | Clear the saved master/cluster pairing and resume searching |
| Hold for 5 seconds | Restart the slave engine without clearing pairing |

## Pairing

1. Configure WiFi and miner pool settings on the screened master.
2. Open **Distributed Miner** on the master and start the cluster.
3. Open **Pair Slaves** and press B1.
4. Power the S3-Zero. Its LED pulses red while searching, then becomes blue or
   green after pairing and receiving work.

If the node belongs to another cluster, triple-tap BOOT to clear its saved
master before opening pairing on the new master.

## Mining Architecture

- Core 1 runs the ESP32-S3 register-level hardware SHA-256d worker.
- The hardware peripheral uses its own first-block midstate representation.
  The complete reference double-SHA is calculated before taking the SHA lock,
  then the first hardware digest is compared against it. A mismatch selects
  software fallback and reports an error state.
- Core 0 optionally runs a baked-software helper. Hardware SHA is a single
  shared peripheral, so the second worker cannot run another hardware engine.
- Work arrives in adaptive multi-second assignments, with one future assignment
  prefetched to reduce idle time.
- ESP-NOW callbacks only enqueue packets. Worker tasks process state changes,
  acknowledgements, retries, cancellation, and duplicate suppression.

To build a hardware-only A/B image:

```powershell
arduino-cli compile --build-property "compiler.cpp.extra_flags=-DMICRI_CLUSTER_S3_SOFTWARE_WORKER=0" --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=4M,PartitionScheme=huge_app,PSRAM=enabled,UploadSpeed=921600 MicriOS-S3-Zero-Headless
```

Keep the software helper only if a five-minute test improves effective cluster
hashrate by at least 5% without additional retries, queue drops, heat, or
instability.

## Serial Diagnostics

Serial runs at `115200` baud. Boot output reports the detected chip, cores,
flash, PSRAM, MAC address, and worker configuration. A status line follows every
two seconds:

```text
[s3zero] build=v3.0 b86 state=Mining paired=yes ch=8 raw_khps=159.0 effective_khps=129.1 sha=hardware jobs=6 done=3 assign=67 progress=827392/1551264 rx_drop=0 result_retry=0 idle_ms=9718 heap=248564 stack=5560 stack2=6284 total_kh=2868 temp_c=65.6 led=off
```

Important fields:

- `sha=hardware`: the hardware digest self-test passed.
- `sha=software-fallback`: hardware validation failed and needs investigation.
- `raw_khps`: active hashing speed inside assignments.
- `effective_khps`: rate including assignment and radio gaps.
- `rx_drop` / `result_retry`: ESP-NOW reliability indicators.
- `idle_ms`: cumulative paired time waiting without work.
- `stack` / `stack2`: FreeRTOS stack high-water marks for the workers.

## Compatibility

The assignment acknowledgements and session IDs use cluster protocol version 3.
Masters and slaves must use the b86-era protocol or newer; incompatible packets
are intentionally ignored.
