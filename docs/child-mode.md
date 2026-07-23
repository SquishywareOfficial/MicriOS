# CYD Child Mode

Child Mode is a CYD parental timer that locks MicriOS at boot until a parent
enters a six-digit PIN and chooses an access duration. It is currently exposed
only by `MicriOS-CYD`; its timer, PIN-entry, and lockout logic live in shared
code so future touch targets can reuse them.

## Setup

1. Open **Settings / Options / Child Mode**.
2. Enter a six-digit parent PIN and enter it again to confirm.
3. Choose **10 minutes**, **20 minutes**, **30 minutes**, **1 hour**,
   **2 hours**, or **Unlimited**.

The timer begins immediately. Unlimited access lasts only until the next
restart. Rebooting never restores an active timed or unlimited session; an
enabled device always boots locked.

## Unlocking

The lock screen masks all entered digits and enables **Unlock** after exactly
six digits. A correct PIN opens the duration choices. An incorrect submitted
PIN clears the entry, sounds one short notification at the saved system volume,
and disables the keypad for 30 seconds.

The hardcoded recovery PIN is `420420`. Child Mode is intended as a practical
parental restriction, not protection against a determined technical attacker.

## Custom Splash

After parental authentication, **Options / Child Mode** provides:

- **Custom Splash**: enables or disables the additional boot page.
- **Customise Splash / Custom Text**: edits a title of up to 24 characters with
  the CYD touch keyboard. The default is `Child's Computer`.
- **Customise Splash / Colour Palette**: selects Candy, Ocean, Space, Forest,
  Sunset, or Rainbow colors.
- **Customise Splash / Preview Splash**: previews the current title and palette.

When enabled, the custom page appears for 2.5 seconds after the MicriOS boot
splash and before the parental PIN page. It can be dismissed with a tap. Splash
settings are stored with the Child Mode configuration, survive PIN changes, and
are removed when Child Mode data is erased.

## During A Session

Timed access counts every unlocked minute, including time spent in menus,
games, paused media, setup portals, BLE tools, WiFi utilities, and miners. The
normal system bar displays an amber `Kid` indicator; immersive apps hide the
bar but do not pause the timer.

Triple-tap the amber `Kid` indicator within 1.5 seconds to lock the device
immediately. The gesture uses the same cleanup and parent-unlock path as a
manual **Lock Now**, and its touches are not passed through to the open app.

At expiry MicriOS:

1. Exits the active app through its normal cleanup lifecycle.
2. Stops pending radio, miner, media, SD, and audio activity owned by that app.
3. Clears input and displays **Time is up** with **Parent Unlock**.
4. Sounds one short notification.
5. After ten seconds, enters the CYD's low-power locked fallback.

The full CYD app suite leaves too little classic-ESP32 IRAM for ESP-IDF's ext0
deep-sleep path: enabling it exceeds IRAM by approximately 520 bytes. The
current fallback turns radios, RGB, and audio off, dims the display, and keeps
polling touch for a parent. A successful parental unlock restores the saved
brightness level before returning to the menu. Deep sleep with touch wake
remains a future IRAM budget task.

## Parent Management

Opening **Options / Child Mode** after setup requires the parent PIN. The
management page provides:

- **Lock Now**: ends the current session immediately.
- **Custom Splash**: toggles the personalized boot page.
- **Customise Splash**: opens text, palette, and preview controls.
- **Change PIN**: replaces the stored salted PIN hash.
- **Disable Child Mode**: erases Child Mode configuration and the PIN.

Authentication is retained only while that management flow remains open.

## Save Manager

Child Mode uses Preferences namespace `kidmode`. Deleting that entry disables
Child Mode and erases its PIN. While Child Mode is enabled, deleting its entry
requires parent authentication. **Delete ALL** also requires the parent PIN
before its existing two confirmation dialogs.
