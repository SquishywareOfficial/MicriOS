# Distributed Miner

Distributed Miner is MicriOS's ESP-NOW cluster mining mode. One screened Micri Deck acts as the master, connects to WiFi and the mining pool, then assigns nonce ranges to nearby slaves over ESP-NOW.

This is separate from **Micri Miner**:

- **Micri Miner**: one device mines directly against the pool.
- **Distributed Miner**: one master coordinates one or more local slave devices.

## Architecture

The master:

- connects to WiFi using saved MicriOS WiFi profiles
- loads saved Micri Miner wallet and pool settings
- connects to the Stratum pool once
- broadcasts a pairing beacon when pairing is enabled
- assigns nonce ranges to paired slaves
- optionally mines locally
- submits valid shares found by itself or by slaves

The slaves:

- do not need WiFi credentials
- do not need wallet or pool settings
- channel-hop until they hear a master pairing beacon
- remember the paired master MAC and cluster ID
- receive compact work assignments
- hash assigned nonce ranges
- return hashrate, best difficulty, and found shares

## Protocol Summary

Distributed Miner uses ESP-NOW packets between the master and slaves. The packet flow is intentionally small:

- master beacon
- slave hello
- slave status
- work assignment
- work result
- cancel/reset signals

Assignments contain the block header, start nonce, and nonce count. Faster
slaves report capability flags and hashrate, and the master gives them larger
work ranges to reduce ESP-NOW overhead. Newer slaves can also hold one queued
next assignment, allowing the master to prefetch work before the current range
finishes.

Cluster protocol version 3 adds application-level acknowledgements for work
assignments and results. Missing acknowledgements are retried at a bounded rate,
duplicate results are ignored by the master, and each master run generates a
session ID so delayed packets from a previous run cannot revive stale work.
ESP-NOW receive callbacks only copy packets into a queue; parsing, Preferences,
and assignment state changes run in the owning miner task.

Pairing uses the master's MAC address plus a persisted cluster ID. Resetting the cluster ID forces slaves to pair again.

## Supported Roles

### T-Display

Open **Distributed Miner**, choose **Master** or **Slave**, then start.

T-Display can act as:

- master
- slave
- master with local mining enabled
- master with local mining disabled

### ESP32-C3 OLED

Open **Distributed Miner**, tap to choose **Master** or **Slave**, then hold to start.

The C3 OLED UI is compact and mainly useful for testing or lightweight slave use.

### ESP32-C3 Headless

Flash the `MicriOS-C3-Headless` build. It has a tiny LED/button launcher with Mouse Emulator and Slave Miner apps. Fresh boards start in the LED menu; open Slave Miner from the menu, or double-tap while Slave Miner is running to save it as the autolaunch app.

To clear a saved headless autolaunch app, return to the LED menu and double-tap. The menu becomes the boot default again.

See [../MicriOS-C3-Headless/README.md](../MicriOS-C3-Headless/README.md) for the headless-specific LED and BOOT behavior.

### ESP32-S3-Zero Headless

Flash the `MicriOS-S3-Zero-Headless` build for ESP32-S3-Zero boards with no
screen. Fresh boards default directly into Slave Miner so a powered cluster node
can immediately search for a Distributed Miner master.

The S3-Zero build uses BOOT on `GPIO0`, treats the onboard WS2812 on `GPIO21`
as a blue status LED, and uses the ESP32-S3 SHA peripheral from a core-1 worker.
A core-0 baked-software helper is enabled by default and can be disabled for
A/B testing. It advertises fast/hardware/dual-worker capability to the master,
receives larger nonce assignments than the C3 headless build, and can buffer
one queued next assignment to reduce idle time between batches.

See [../MicriOS-S3-Zero-Headless/README.md](../MicriOS-S3-Zero-Headless/README.md)
for S3-Zero LED, BOOT, compile, and upload details.

## Typical Setup

1. Flash the master with **MicriOS for T-Display** or **MicriOS for C3**.
2. Configure WiFi on the master through **WiFi Setup**.
3. Configure wallet and pool settings through **Micri Miner** setup if you do not want the defaults.
4. Flash slave boards with **MicriOS for C3 Headless** or **MicriOS for S3-Zero Headless**, or launch **Distributed Miner** in **Slave** mode on screened boards.
5. Launch **Distributed Miner** on the master in **Master** mode.
6. Start the cluster.
7. Open the pairing page and start **Pair Slaves**.
8. Wait for slaves to appear in the slave list.
9. Turn **Local Mining** on or off depending on whether the master should also hash.

## T-Display Master Controls

- **Dashboard**: Button 1 starts/stops the cluster. Button 2 changes page.
- **Pair Slaves**: Button 1 starts pairing.
- **Cluster Control**: Button 1 toggles local mining. Button 1 hold starts/stops the cluster.
- **Reset Cluster**: Button 1 resets the cluster ID. Slaves must pair again.
- Button 2 hold exits to the MicriOS menu.

## T-Display Slave Controls

- Button 2 changes status/detail pages.
- **Clear Pairing**: Button 1 hold forgets the saved master.
- **Exit Slave**: Button 1 hold exits to the MicriOS menu.
- Button 2 hold also exits to the menu.

## Headless C3 LED And Button

The headless C3 uses GPIO8 for status on tested ESP32-C3 SuperMini boards. The LED first shows which app is active, then shows that app's state pattern.

For Slave Miner:

- App marker: LED on, two short blinks, LED on again, short off gap.
- Solid LED for up to 5 seconds: actively or recently mining.
- Slow blink for up to 5 seconds: searching / not paired.
- Very slow blink for up to 5 seconds: paired but idle.
- Fast blink for up to 5 seconds: error.
- 2 taps: make Slave Miner the saved autolaunch app.
- 3 taps: clear saved master/cluster pairing.
- Hold BOOT for about five seconds: exit to the LED menu.
- In the LED menu, 2 taps clears the saved autolaunch app and keeps future boots in the menu.

The tested GPIO8 LED is active-low. Other boards may need LED polarity changes.

## Headless S3-Zero LED And Button

The S3-Zero headless target uses BOOT on `GPIO0` and an onboard WS2812 RGB LED
on `GPIO21`. MicriOS drives that RGB LED as a blue status light.

Fresh boards boot straight into Slave Miner. Hold BOOT for about five seconds to
return to the LED menu.

For Slave Miner:

- App marker: LED on, two short blinks, LED on again, short off gap.
- Solid LED for up to 5 seconds: actively or recently mining.
- Slow blink for up to 5 seconds: searching / not paired.
- Very slow blink for up to 5 seconds: paired but idle.
- Fast blink for up to 5 seconds: error.
- 2 taps: make Slave Miner the saved autolaunch app.
- 3 taps: clear saved master/cluster pairing.
- Hold BOOT for about five seconds: exit to the LED menu.

In the LED menu, 2 taps clears the saved autolaunch app. On S3-Zero, that means
the board returns to its factory behavior of booting straight into Slave Miner.

## Serial Debug

T-Display master serial runs at `115200` baud:

```text
cluster start
cluster stop
cluster stats
cluster pair
cluster reset
cluster local on
cluster local off
```

`cluster stats` prints total, local, and slave hashrate, slave count, jobs, submitted/accepted/rejected shares, channel, and last error.

Headless C3 and S3-Zero serial also run at `115200` baud and print status every two seconds:

```text
[headless] build=<build> state=<state> paired=<yes/no> channel=<n> khps=<rate> jobs=<jobs> completed=<count> total_kh=<hashes>
```

## Clearing Saved Cluster Data

On MicriOS builds, use:

**Options / Save Manager / Cluster**

On headless C3 or S3-Zero, triple-tap while Slave Miner is running.

## Performance Notes

- T-Display/classic ESP32 slaves use the ESP32 hardware SHA path.
- ESP32-C3 slaves use the C3-compatible path and are much slower than T-Display/classic ESP32 boards.
- ESP32-S3-Zero slaves use a dedicated register-level S3 hardware SHA path,
  larger batches, queued next-assignment prefetch, and effective-hashrate
  reporting. The first hardware digest is checked against the baked software
  implementation; validation failure automatically selects software fallback.
- S3 hardware is one shared accelerator, not one accelerator per CPU core. The
  default second worker is software-only and should be retained only if hardware
  measurements show a useful effective-hashrate gain.
- Local mining can be disabled on the master if you want the master to focus on coordinating slaves.
- Larger nonce assignments improve fast slave throughput by reducing ESP-NOW assignment overhead.
- Very large assignments reduce radio overhead, but they also increase the
  amount of stale work a slave may finish after the pool issues a new job. The
  current S3 target aims for multi-second batches rather than tiny interactive
  batches because the headless slaves do not need UI responsiveness.
- S3 slave cancellation checks are tuned from the last observed hashrate. The
  hot loop checks for cancels/new-job invalidation roughly every 50 ms, clamped
  to a power-of-two stride between 1,024 and 65,536 nonces so fast nodes avoid
  excessive polling overhead.
- Busy S3 workers perform a real one-tick delay at least every 50 ms so WiFi,
  idle/watchdog, serial, LED, and BOOT handling remain serviceable.
- Protocol-v3 masters and slaves must be upgraded together. Older cluster
  protocol builds intentionally reject the new packet layout.

## Troubleshooting

- If a slave says it is paired but the master shows no slaves, reset cluster data on both devices and pair again.
- If the master is on channel `0`, start the cluster before pairing.
- If slaves show paired but `0H/s`, check that the master is connected to WiFi and the pool.
- If a headless C3 Slave Miner state pattern is blinking slowly forever, it is probably searching for a master beacon.
- If you change the master device, clear slave pairing before pairing with the new master.
