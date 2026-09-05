# Battery Telemetry And Session Tracking

MicriOS separates reusable battery-session accounting from each board's
electrical implementation.

- `shared/logic/BatterySessionLogic.h` owns source classification, session
  duration, minimum voltage, interrupted-session handling, and checkpoint
  scheduling.
- Each hardware target remains responsible for obtaining a trustworthy battery
  voltage, persisting records, rendering its UI, and implementing safe sleep.
- The first concrete implementation is the classic T-Display target in
  `MicriOS-T-Display/TDisplayPower.*`.

## T-Display Sampling

When **Battery Installed** is enabled, the T-Display samples its switched
GPIO34 battery divider every 30 seconds. A sample takes approximately 56 ms:
8 ms of divider settling plus 24 spaced ADC samples. Most of that time yields
to the scheduler, and the same sample supports percentage display, source
detection, session tracking, minimum-voltage tracking, and low-battery safety.

Invalid samples are ignored. They never become zero volts and never trigger a
low-battery shutdown.

## USB And Battery Detection

The shared classifier uses hysteresis:

- Above `4.50 V`: USB/charger power.
- Below `4.40 V`: battery power.
- From `4.40 V` through `4.50 V`: retain the previous source.

These values describe the tested T-Display charger-rail measurement. A future
board must validate its own divider and power-path behavior before reusing the
same thresholds.

## Session Records

Battery sessions record:

- Elapsed powered-on battery minutes.
- Lowest raw voltage observed.
- Last raw voltage observed.
- Whether the record ended cleanly.
- Whether a reset or power interruption made the duration approximate.
- End reason: USB connected, manual sleep, low-battery sleep, disabled, or
  interrupted.

The active record is checkpointed every 30 minutes. A new active record is also
written when a battery session begins, so a reset before the first checkpoint
is still recognizable.

A session is finalized immediately when:

- USB power returns.
- The user selects **Deep Sleep** from **Sleep Device**.
- Critical low-battery protection enters deep sleep.
- Battery support is disabled.

T-Display Preferences use namespace `power`:

- `bcur`: active, incomplete battery-session checkpoint.
- `blast`: most recently finalized or recovered session.
- `battery`: user-declared battery-installed setting.
- `fullmv`: user-calibrated full-voltage reference for percentage display.
- `bright`: saved backlight level.

Save Manager's **Power** entry clears these values and resets the in-memory
tracker.

## Interrupted Sessions

The active checkpoint is deliberately stored as incomplete. If RST, a crash,
or sudden power loss occurs, the next boot recognizes it:

- If the device is still on battery, the session resumes from the last
  checkpoint and remains marked interrupted.
- If the device boots on USB, the checkpoint becomes the last interrupted
  session.

The UI adds `*` to affected duration and minimum-voltage values. Because
checkpoints occur every 30 minutes, an interrupted duration can be short by up
to 30 minutes. A later clean sleep does not remove that asterisk from a session
that was interrupted earlier.

## Screen Standby

The T-Display **Sleep Device** menu also offers **Screen Off**. This is a
reversible display-only standby:

- GPIO4 backlight PWM is set to zero.
- The ST7789 receives `DISPOFF` and `SLPIN`.
- CPU state, RAM, timers, the active app, WiFi, Bluetooth, and ESP-NOW remain
  untouched.
- App logic may continue ticking, but rendering is paused.
- Either button wakes the panel, restores saved brightness, consumes the wake
  input, and requests a deterministic redraw.

Screen standby does not finalize the battery session because the device remains
running. The target-local `TDisplayPower::enterScreenStandby()` and
`exitScreenStandby()` functions intentionally own only display hardware so a
future inactivity timer or foreground app can reuse the same lifecycle.

## Automatic Idle Display

**Power Settings / Idle Display** exposes three independent persisted timers:

- **Saver After** defaults to five minutes, works on USB or battery, and may be
  disabled.
- **Battery Dim** defaults to Off and applies brightness level 1 only when
  Battery Installed is enabled and telemetry identifies Battery power.
- **Screen Off** defaults to fifteen minutes, is honored only while telemetry
  identifies Battery power, and may be disabled.

All three timers measure from the same last physical button interaction. With
the defaults on battery, the selected saver starts at minute five and screen
standby begins at minute fifteen. If Battery Dim is enabled, its threshold uses
that same idle origin; later timers do not restart after an earlier action. Idle time accrues only in
MicriOS shell menus and while a saver is playing. Ordinary apps and games
continually suspend/reset idle accounting so returning from a long-running app
cannot immediately blank the display.

Any button restores the configured brightness before its input is handled.
Brightness also restores when telemetry changes from Battery to USB. If the
screen subsequently enters standby, wake restores the configured brightness
through the existing standby lifecycle.

The Screen Saver picker stores its default in Preferences when B1 is held to
start the displayed saver. B1/B2 browse the choices. During actual playback,
either button exits directly to the MicriOS root menu and its input is consumed.
The **Micri Clock** saver is display-only: it uses Micri Clock's saved timezone,
12/24-hour, date, and offset preferences, but does not expose Clock menus or
start WiFi. If system time has not been synchronized, it reports that time is
unavailable.

At the Screen Off threshold, an active saver is exited through the normal app
lifecycle before the existing reversible display standby is entered. WiFi,
Bluetooth, ESP-NOW, timers, and battery telemetry retain the same behavior as a
manual **Screen Off**. Either button wakes the panel and resets the idle timer.

The target-local namespace `displayidle` stores `savermin`, `dimmin`, `offmin`,
and the selected saver. Save Manager's **Idle Display** entry clears them without
altering battery telemetry in the `power` namespace. Timeout decisions are
hardware-independent in `shared/logic/IdleDisplayLogic.h`; persistence, screen
saver launch, and panel standby remain target-owned.

## Uptime And Deep Sleep

Boot uptime and current-session time use `esp_timer_get_time()` and minute
granularity. Tracking itself costs only a few bytes of RAM and occasional
integer arithmetic.

The T-Display Battery Runtime page always shows this monotonic uptime. When the
system clock has a valid NTP-derived epoch, it also derives the local boot
timestamp as `current epoch - boot uptime` and formats it with Micri Clock's
active UTC offset. This also works when the first successful sync occurs after
boot. If no trustworthy epoch is available, the boot timestamp is shown as
unavailable rather than treating an old persisted timestamp as current time.

Deep-sleep time is not counted. The high-resolution monotonic timer stops, and
RST/EN wakes the current T-Display implementation as a new boot. The battery
session is finalized before the display and radios are shut down.

## Porting To Another Board

Reuse `BatterySessionLogic::Tracker`, but implement these board-specific parts:

1. Verify the battery ADC pin, divider ratio, enable polarity, attenuation, and
   calibration against a multimeter.
2. Establish reliable USB-versus-battery thresholds for that board's power
   path.
3. Feed only valid raw millivolt samples into `observeVoltage()`.
4. Persist `SessionRecord` as an atomic blob and validate its checksum/schema.
5. Consume `SaveCurrent` and `FinalizeToLast` persistence actions.
6. Call `finish()` before controlled sleep or shutdown.
7. Keep raw-voltage safety thresholds independent from percentage calibration.
8. Ensure the board's Save Manager clears both persistent records and the
   in-memory tracker.
9. Render cached telemetry rather than causing additional ADC samples merely
   to refresh a screen.

Percentage curves, warning thresholds, sleep pins, and wake sources remain
hardware-specific unless measurements prove they can be shared safely.
