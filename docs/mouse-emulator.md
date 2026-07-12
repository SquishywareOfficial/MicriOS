# Mouse Emulator

Mouse Emulator is MicriOS's Bluetooth HID mouse utility. It advertises as a real mouse profile, pairs with a host computer, and performs occasional movement sweeps with small vertical variation.

## Supported Builds

- MicriOS for C3
- MicriOS for T-Display
- MicriOS for C3 Headless

The headless C3 build includes a no-screen Mouse Emulator app with fixed Logitech identity and LED/button controls.

## Device Profiles

The selected identity is saved in the `mouse` Preferences namespace and reused next time.

Available profiles:

- Logitech: `Logitech Signature M650`
- Lenovo: `ThinkPad Wireless Mouse`
- Dell: `MS5320W Multi-Device`
- Generic: `Wireless Mouse`
- Apple: `Apple Magic Mouse`

Each profile sets the Bluetooth device name, manufacturer, and USB HID vendor/product IDs used in the HID PnP metadata.

## Pairing

1. Open **Mouse Emulator**.
2. Choose the identity profile.
3. Start the app.
4. On the host computer, open Bluetooth settings.
5. Pair with the advertised mouse name.

The app uses NimBLE and presents as a Bluetooth HID mouse with no PIN entry.

## Controls

### MicriOS for C3

On the profile screen:

- Tap: cycle to the next mouse identity.
- Hold: start advertising as the selected identity.

While running:

- Tap: toggle movement active/paused.
- Hold: exit to menu and stop Bluetooth advertising.

### MicriOS for T-Display

On the profile screen:

- Button 1: cycle to the next mouse identity.
- Button 2 or Button 1 hold: start advertising as the selected identity.

While running:

- Button 1: toggle movement active/paused.
- Button 2 hold: exit to menu and stop Bluetooth advertising.

### MicriOS for C3 Headless

The headless build always uses the Logitech profile and has no profile picker.

- In the LED menu, 2 taps clears the saved autolaunch app and makes the LED menu the boot default again.
- 1 tap: no action, to avoid accidental taps.
- 2 taps: make Mouse Emulator the saved autolaunch app.
- 3 taps: clear saved Bluetooth mouse pairing, rotate the BLE address, and return to advertising / pairing mode.
- Hold BOOT for 5 seconds: exit to the LED menu and stop Bluetooth advertising.

## Movement Behavior

When paired and active, Mouse Emulator waits a random interval before moving:

- interval: 10-50 seconds
- movement length: 500-5000 pixels
- pause before returning: 400-1200 ms
- vertical wiggle limit: +/-20 pixels

Movement is sent as small HID mouse steps. The app moves forward, waits briefly, then moves back by the same amount so the pointer roughly returns to where it started.

## Display Status

The C3 OLED shows compact pairing/active state and a progress bar toward the next movement.

The T-Display version shows:

- advertised profile
- host pairing state
- active/paused state
- countdown to next movement
- last movement distance

The headless version uses GPIO8 LED cycles:

- App marker: LED on, one short blink, LED on again, short off gap.
- Solid on for 5 seconds: connected and active.
- Solid on 5 seconds, then one blink: connected but paused.
- Solid on 5 seconds, then two blinks: not connected / Bluetooth unavailable.
- Solid on 5 seconds, then three blinks: advertising / pairing mode.

## Notes

- Leaving the app stops Bluetooth advertising and deinitializes NimBLE.
- If the host reports a driver or pairing error, triple-tap in the headless Mouse Emulator app to clear the device-side bond and rotate the BLE address, then forget/remove the old Bluetooth device on the host and pair again.
- If changing profiles, pair again under the new advertised device name.
