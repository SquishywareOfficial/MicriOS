# Release Notes

## v3.0 b86

- Added `MicriOS-S3-Zero-Headless` for ESP32-S3-Zero no-screen boards with BOOT on `GPIO0` and onboard WS2812 status LED on `GPIO21`.
- Made the S3-Zero headless build default straight into Distributed Miner Slave Miner mode on fresh boards.
- Added an ESP32-S3 register-level hardware SHA worker with a first-use
  software cross-check and automatic software fallback; the optional core-0
  baked-software helper remains available for measured A/B testing.
- Tuned the distributed miner slave engine so ESP32-S3 targets advertise fast/hardware/dual-worker capability, accept larger nonce assignments, and buffer one queued assignment so the master can prefetch the next batch before the current one finishes.
- Added effective hashrate reporting for prefetched slave work so the master can distinguish raw hashing speed from assignment/ESP-NOW overhead.
- Tuned S3 slave cancellation checks from observed hashrate so high-speed slaves do less hot-loop polling while still responding to new jobs/cancels on a bounded cadence.
- Moved cluster ESP-NOW receive work out of WiFi callbacks into bounded task
  queues, added assignment/result acknowledgements and retries, suppressed
  duplicate results, and added per-master session IDs plus job-aware cancels so
  delayed packets cannot replace or cancel current work.
- Added cooperative worker delays so the headless S3 miner does not starve
  WiFi, idle/watchdog, serial, LED, or BOOT processing while hashing.
- Added S3-Zero build, release packaging, web flasher manifest, and documentation entries.

## v3.0 b85

- Renamed the active firmware/software branding from Femto OS to `MicriOS`, with flashed devices now described as `Micri Deck` / `MicriDeck`.
- Renamed the Arduino targets, sketches, build artifacts, web installer manifests, simulator text, current docs, and firmware-visible About/Credits text to the `MicriOS-*` naming.
- Renamed active setup names and compatibility strings including WiFi APs/passwords, miner setup AP/password, miner worker prefix, hostnames, Stratum user-agent labels, and related task labels.
- Added migrations for old autolaunch app names and the previous `femtofld` Micri Field score namespace.
- Updated the current docs, web flasher, simulator, and firmware-visible repo pointers for the new `SquishywareOfficial/MicriOS` project home.

## v2.1 b84

- Added `Femto Casino` on C3 and T-Display, replacing the standalone Blackjack menu entry with a shared casino hub.
- Added shared casino logic for Blackjack, Roulette, Slots, Texas Hold'em, and Video Poker, with a persistent bankroll, per-game betting, cash-out, and reset flow.
- Improved casino presentation with T-Display card/suit rendering, slot spin/win highlighting, roulette spin feedback, and clearer Hold'em street/dealer labels.
- Added a T-Display `Tong-its` first pass with shared rummy logic, draw/discard flow, meld exposure, discard-pickup enforcement, sapaw/layoff onto exposed melds, bot turns, and stock-empty deadwood scoring.
- Added game guide documentation for Tong-its and linked game guides from the README/docs index.

## v2.1 b83

- Stabilized T-Display rendering by moving the main app and game draw paths onto a single shared reusable full-screen framebuffer.
- Fixed T-Display Femto Clock, Countdown, and Metronome live updates so recurring ticks/pulses render as complete buffered frames instead of live partial clears.
- Reduced T-Display tearing/flicker across games, utilities, settings/about surfaces, miners, Reading, Communicator, Mouse Emulator, Pet Simulator, and WiFi Setup screens.
- Removed stray Alien Raiders intro tether lines behind the launch ship.

## v2.1 b82

- Moved the canonical project home to `SquishywareOfficial/FemtoOS`.
- Updated README, flashing docs, web installer links, simulator metadata, and firmware About pages to point to the new repository and Pages URLs.

## v2.1 b81

- Renamed the firmware/software branding to `Femto OS`, with `FemtoOS-*` target folders/artifacts and `FemtoDeck` retained only for device-style splash branding and compatibility names.
- Renamed the Arduino target folders and sketch files to `FemtoOS-C3`, `FemtoOS-T-Display`, and `FemtoOS-C3-Headless`.
- Updated build scripts, GitHub Actions, web installer manifests, simulator text, README, and docs for the new Femto OS naming.

## v2.0 b80

- Expanded T-Display Screen Saver with Matrix Rain, Plasma, Fire, Bouncing Balls, Bezier Ribbons, Lissajous, Snowfall, Fireworks, Hyperspace Grid, Radar Sweep, Kaleidoscope, Spirograph, Sandstorm, and Night Drive.
- Expanded C3 Screen Saver with Matrix Rain, Snowfall, Lissajous, Radar Sweep, and Fireworks.

## v2.0 b79

- Added a Screen Saver utility on T-Display and C3.
- Added T-Display screen savers inspired by classic Windows effects: starfield, longer-running connected 2D pipes, stargate tunnel, swirling lines, fractal zoom, and bouncing `Femto OS` text.
- Added compact C3 screen savers for starfield, bouncing `Femto OS` text, and swirling lines.
- Wired Screen Saver into the Apps/Utilities menus and T-Display autolaunch choices.

## v2.0 b78

- Changed the headless ESP32-C3 build from a single-purpose miner slave into a tiny LED/button launcher with Mouse Emulator and Slave Miner apps.
- Added a headless Bluetooth Mouse Emulator that advertises as `Logitech Signature M650`, uses the shared humanized movement logic, and can be selected as the saved autolaunch app.
- Added headless LED menu and app-state patterns, including app markers, autolaunch selection by double tap, miner pairing reset by triple tap, and 5-second hold-to-menu behavior.
- Changed fresh headless ESP32-C3 boards to start in the LED menu instead of defaulting to Slave Miner autolaunch.
- Added headless Mouse Emulator triple-tap pairing reset, which clears stored BLE bonds, rotates the BLE address, and restarts advertising.
- Added a short off separator between the headless app marker blink and the app state pattern so solid-on states do not visually merge together.
- Added LED-menu double-tap on headless C3 to clear the saved autolaunch app, making the LED menu the boot default again.
- Improved T-Display WiFi Setup AP startup by fully cycling WiFi before AP mode, increasing AP TX power, and allowing 802.11n to reduce client-side "incorrect password" handshake failures.
- Restored the direct NimBLE UUID link anchor in the T-Display build so Mouse Emulator BLE symbols link reliably with Arduino's dependency scanner.

## v2.0 b77

- Added T-Display slave mode to the Distributed Miner app, with a role selector before launch.
- Added native T-Display slave pages for status, work rate, assignment debug data, pairing reset, and exit.
- Kept T-Display serial cluster commands master-oriented by forcing the app back to master mode before debug start/pair/reset actions.
- Tuned Distributed Miner nonce assignments so fast slaves receive larger work ranges based on their reported hashrate, reducing ESP-NOW assignment overhead.
- Renamed the T-Display cluster utility internals to `Distributed Miner` so C3 and T-Display use the same user-facing app name.
- Updated About / License and Credits so NerdMiner-derived MIT portions, Femto Miner, and Distributed Miner are represented on device.

## v2.0 b76

- Changed T-Display City Racer to keep its framebuffer once allocated instead of releasing and reallocating it between runs.
- Added a 4-bit full-screen sprite fallback for City Racer so it still avoids direct TFT drawing if the 8-bit framebuffer cannot be allocated.
- Removed redundant per-frame sprite clearing in City Racer and added serial diagnostics showing the sprite depth in use.
- Added automatic Femto Miner retry/backoff after WiFi, pool connection, subscribe, or pool disconnect failures so transient errors recover without manual restart.
- Fixed Distributed Miner slave pairing by refreshing the ESP-NOW master peer when the slave hops to the master's WiFi channel, allowing the master to receive C3 hello/status packets and assign work.
- Changed the T-Display `cluster pair` debug command to start the cluster engine before opening pairing, avoiding an idle `channel=0` pairing state.
- Updated the headless C3 miner LED behavior for active-low GPIO8 boards and documented the LED states, BOOT pairing reset, and serial status output.

## v2.0 b75

- Fixed T-Display City Racer falling back to tearing-prone direct TFT drawing after a one-time sprite allocation failure.
- Changed City Racer to retry its 8-bit sprite framebuffer, clear the sprite before each frame, and release the sprite when leaving the app.

## v2.0 b74

- Fixed T-Display Distributed Miner page rendering so dashboard, slave list, pool, pairing, and control pages clear through the same sprite-backed pattern as Femto Miner.
- Added immediate redraw and forced clear handling to Distributed Miner page/action changes to remove stale text between pages.
- Clarified the Distributed Miner pairing screen so it shows when pairing is requested but the ESP-NOW radio is still waiting for WiFi/radio startup.

## v2.0 b73

- Clarified T-Display Femto Miner dashboard and Shares page wording so pool jobs are shown separately from submitted/accepted/rejected shares.
- Forced share counters to render explicit zero values, avoiding the impression that received pool jobs should each have an OK/rejected status.

## v2.0 b72

- Changed the T-Display Femto Miner BTC price flow so it automatically tries to fetch a cached price when entering the BTC page.
- Changed T-Display Femto Miner start/autostart to attempt a BTC price fetch before starting the CPU-intensive mining workers.
- Added CPU temperature to the T-Display Femto Miner Session page and optional serial miner logs.
- Tuned T-Display Femto Miner workers for better button responsiveness while mining by reducing nonce batch sizes and lowering worker priorities.
- Hardened T-Display Femto Miner rendering so the framebuffer retries allocation, clears before each page draw, and autostart mining waits until after the first UI frame.

## v2.0 b71

- Expanded the T-Display Femto Miner into multiple status pages for dashboard rate, shares/jobs, session totals, lifetime totals, pool state, BTC price lookup, setup, and reset.
- Added persisted T-Display miner lifetime totals for hashes, mining duration, jobs, submitted shares, accepted shares, and rejected shares.
- Added a manual BTC price lookup page using the public mempool.space prices endpoint when WiFi is connected.
- Improved T-Display Femto Miner page transitions by adding an immediate-render hook and forcing a panel clear on miner mode/page changes.
- Changed T-Display miner serial hashrate logging to opt-in with `miner logs on|off`; `miner stats` remains available for manual snapshots.
- Added `Miner Stats` to the T-Display Save Manager so persisted miner totals can be cleared separately from wallet/pool settings.

## v2.0 b70

- Added a T-Display **Distributed Miner** utility for ESP-NOW cluster mining, separate from the existing solo Femto Miner app.
- Added shared cluster protocol and mining logic for master beacons, pairing, slave status, nonce-range assignments, work results, local master mining, and pool share submission.
- Added a generic `femto-c3-headless` ESP32-C3 sketch; its first role is a no-display Miner Slave with GPIO8 LED status and BOOT long-hold pairing reset.
- Added a persisted cluster `Local Mining` setting so the T-Display master can mine locally or act mostly as a coordination node during testing.
- Added `cluster start`, `cluster stop`, `cluster stats`, `cluster pair`, and `cluster local on|off` serial debug commands.
- Added the headless C3 build to CI artifacts and the browser installer manifest.

## v2.0 b69

- Replaced the T-Display debug-only autolaunch preference with a generic `Autolaunch` Settings app.
- Added persisted T-Display autolaunch fields for enabled/disabled, selected app/game, and generic auto-run behavior.
- Migrated the previous debug `auto_app` / `auto_miner` preferences into the new `autolaunch` namespace so existing Femto Miner autolaunch settings survive the upgrade.
- Changed T-Display boot flow so the FemtoDeck splash always appears first, followed by a cancellable `Autolaunching <app>` screen when autolaunch is enabled.
- Kept serial autolaunch commands as shortcuts, but changed status output to `autolaunch_enabled`, `autolaunch_app`, and `autolaunch_autorun` instead of miner-specific wording.

## v2.0 b68

- Added a second Femto Miner worker on classic ESP32/T-Display: the main miner task keeps the hardware SHA path while a secondary software worker searches a separate nonce range and queues found shares back to the Stratum owner.
- Kept ESP32-C3 on the single-worker path so the one-core board does not lose UI/WiFi responsiveness to an extra mining task.
- Tuned T-Display miner task priorities after hardware tests; the stable unpinned worker configuration measured about `355 KH/s`, up from the previous `318-319 KH/s`.
- Tested pinned software-worker variants and rejected them: core 0 reduced throughput, while core 1 starved UI/serial stats.

## v2.0 b67

- Added a T-Display debug autolaunch preference so the firmware can boot straight into a named app for serial profiling.
- Added serial debug commands for listing launchable apps, setting/clearing autolaunch, launching an app, and starting/stopping Femto Miner.
- Added Femto Miner serial stats output once per second while mining, including state, hashrate, totals, shares, uptime, pool, and last error.
- Added a persisted Femto Miner autostart flag for benchmarking, plus B2-at-boot autolaunch suppression as a recovery path.

## v2.0 b66

- Reworked Femto Miner hashing to use NerdMiner's baked SHA path instead of generic per-hash mbedTLS setup.
- Added a classic ESP32 hardware-SHA fast path for T-Display mining, matching NerdMiner's SHA peripheral approach much more closely.
- Batched Femto Miner hash stats updates so the worker no longer enters a critical section for every nonce.
- Fixed T-Display Femto Miner page ghosting by forcing landscape rotation before rendering/pushing the miner dashboard sprite.

## v2.0 b65

- Added Femto Miner as a native utility on both T-Display and C3, with shared miner settings, worker-name generation, Stratum connection handling, cooperative SHA mining loop, and live status stats.
- Added a dedicated Femto Miner setup portal for wallet, pool host, port, and pool password settings, using `FemtoMiner Setup` instead of replacing the existing WiFi Setup flow.
- Added on-device Miner settings reset screens on both board targets.
- Added Miner settings to Save Manager and documented the default pool, wallet, worker naming, and NerdMiner_v2 MIT attribution.
- Added ArduinoJson to the documented and CI-installed Arduino library dependencies for Stratum message parsing.
- Added the `onAppExit()` lifecycle hook so apps with background workers, sockets, APs, or radio sessions can stop cleanly when exiting to the menu.

## v2.0 b64

- Reworked T-Display Breakout '76 running rendering to use an 8-bit sprite framebuffer, reducing visible full-screen tearing during play.
- Changed T-Display Breakout '76 paddle controls so B1 moves right only while held and B2 moves left only while held.

## v2.0 b63

- Adjusted T-Display Large text menus to show three clean rows instead of clipping a fourth row under the footer.

## v2.0 b62

- Added a T-Display-only `Text Size` option in Options, defaulting to Compact, with Large available for the core menu surfaces and Reading app.
- Unified T-Display root, About, Options, Save Manager, and WiFi Setup list screens around a shared compact/large menu renderer.
- Fixed T-Display Reading pagination so passage pages are wrapped dynamically by measured text width and never draw under the footer.
- Reduced T-Display Save Manager tearing by redrawing only changed rows when selection moves without scrolling.

## v2.0 b61

- Reduced T-Display Femto Clock tick flicker further by using partial redraws for the digital, analog, and Unix clock faces instead of pushing a full-screen frame every second.

## v2.0 b60

- Applied sprite-backed rendering to the T-Display Femto Clock app so once-per-second clock face redraws no longer visibly tear or flash.

## v2.0 b59

- Applied sprite-backed rendering to the T-Display Options app to reduce visible tearing/black artifacts when switching pages and save-manager rows.
- Added a `Launching WiFi AP` screen before T-Display WiFi Setup begins the slower scan/AP startup work, so Configure no longer appears frozen while the access point starts.

## v2.0 b58

- Applied the City Racer sprite-frame rendering pattern to the T-Display shell menu so Games, Apps, Settings, and About menu page changes are drawn offscreen and pushed as complete frames to reduce tearing.

## v2.0 b57

- Replaced the T-Display WiFi Setup placeholder with a real Settings flow that opens immediately, supports B1 next / B2 previous navigation, and uses B1 hold to open Configure Portal, Test Saved, and Delete Saved actions.
- Added T-Display WiFi Setup portal hosting, saved-profile testing, delete confirmations, and status/message screens while keeping redraws dirty-state based to avoid TFT flicker.

## v2.0 b56

- Reworked T-Display City Racer running rendering to use a sprite-backed portrait frame instead of slowing down full-screen direct redraws, reducing tearing while restoring smoother motion.

## v2.0 b55

- Fixed Blackjack betting controls on both C3 and T-Display so tapping deals the selected bet and holding changes the bet.
- Made natural Blackjack settle immediately after the initial deal, paying 3:2 unless the dealer also has a natural.
- Reduced T-Display Blackjack flicker by skipping redraws when the table, bet, bankroll, and hand state have not changed.
- Tuned T-Display Alien Raiders portrait controls, boss movement, and best-score layout.
- Tuned T-Display City Racer portrait controls and running redraw cadence to reduce screen tearing.

## v2.0 b54

- Reduced T-Display flicker across About, Options, Save Manager, Reading, Communicator, Mouse Emulator, and the small utility apps by avoiding full-screen redraws when screens have not changed.
- Made T-Display Countdown show a full-screen flashing `TIME UP` alert when the timer reaches zero.
- Restored Random Number on both C3 and T-Display to the simpler include-zero plus fixed upper-bound range picker.
- Made the T-Display Metronome beat much more visible with a large central screen pulse instead of a small circle behind the BPM text.
- Polished T-Display Coin Flipper and Dice Roller so the coin reveals in the same spot it spins and tapping after a dice roll rerolls the same die.
- Added Button 2 reverse-direction controls for T-Display Breakout '76, City Racer, and Alien Raiders.
- Flipped the T-Display Cave Chopper sprite so the helicopter faces its direction of travel.

## v2.0 b53

- Improved Femto Clock so it can keep displaying time from the last successful sync when WiFi or NTP is unavailable.
- Added a last-sync age label to Clock faces so stale time is visible instead of silently pretending to be freshly synced.
- Added up to four saved Clock timezone offsets with next/edit/add/delete controls shared across C3 and T-Display builds.

## v2.0 b52

- Added Femto Clock to both C3 and T-Display builds with shared clock settings/formatting logic.
- Clock startup now checks saved WiFi profiles, shows a clear no-saved-WiFi path pointing users to WiFi Settings, connects to the first working saved network, and syncs time using NTP.
- Added Digital, Analog, Unix Time, and Options clock faces with persisted date, 24-hour, UTC offset, and timezone settings.

## v2.0 b51

- Reworked Pet Simulator care balance on both C3 and T-Display so stats decay much more slowly and health only drops after sustained poor care.
- Added visible floor poop, low-health warning flashes, pet death/restart flow, and a short play animation where the pet chases a toy.
- Added care tradeoffs: cleaning can restore health but lowers fun, play raises fun while using hunger/energy and can make the pet dirty.

## v2.0 b50

- Added a FemtoDeck T-Display boot splash that shows the board model and build string before opening the menu.
- Made the T-Display boot splash skippable while consuming the skip button release so it does not immediately move the menu selection.

## v2.0 b49

- Fixed T-Display game resets caused by C3-only GPIO8 LED feedback touching unsafe classic ESP32 flash pins; T-Display LED calls are now no-ops for Femto Field, Fishing Flick, Mini Lander, and Need Speed.
- Reduced T-Display flicker by throttling active app rendering and avoiding redundant redraws of static Start/End screens.
- Fixed T-Display app-exit input leakage so the same Button 2 release no longer immediately moves backward through the menu.
- Replaced the T-Display Communicator placeholder with a compact ESP-NOW send/inbox flow and a real splash screen.
- Added a direct NimBLE service link anchor for the T-Display Mouse Emulator build so BLE HID and ESP-NOW can compile together reliably.

## v2.0 b48

- Continued the T-Display native layout pass across the remaining games, using full 240x135 color screens and deterministic clearing.
- Reworked T-Display Fishing Flick, Knife Throw, Noon Shooter, Mini Lander, Need Speed, Blackjack, Breakout '76, Maze Runner, Maze Collector, Pipe Mania, Tiny Golf, Alien Raiders, and Femto Field.
- Expanded T-Display gameplay geometry where required so sprites, lanes, cards, mazes, golf courses, pipes, and field-event screens use the larger display instead of staying C3-sized.
- Polished the T-Display Mouse Emulator dashboard with clearer connection, enabled, countdown, and movement status rows.
- Updated `TODO.txt` so all games and Mouse Emulator are marked as having a native T-Display pass; WiFi Setup and Communicator remain open for a real radio-app parity port.

## v2.0 b47

- Added a T-Display native port tracker to `TODO.txt` so future sessions can resume app-by-app polishing without rediscovering port status.
- Added a small T-Display UI helper for cleared screens, headers, footers, clipped labels, selected rows, centered values, status pills, and bars.
- Reworked T-Display Counter, Stopwatch, Countdown, Dice Roller, Coin Flipper, Random Number, and Metronome with full-screen color layouts and clearer Button 2 behavior where useful.
- Reworked T-Display Cave Chopper, Tower Stacker, Simon, Reactor, and City Racer with larger 240x135 playfields, readable score/status displays, and color presentation.
- Kept the C3 gameplay/code path intact while continuing the incremental T-Display native layout pass.

## v2.0 b46

- Fixed the T-Display build so TFT_eSPI uses the repo-local `tft_setup.h` without shadowing the library driver definitions.
- Updated C3 and T-Display builds to use the `huge_app` partition in local docs/scripts and GitHub Actions.
- Restored the T-Display root menu structure with Games, Apps, Settings, and About sections.
- Fixed T-Display menu row clearing and scrolling so shorter labels do not inherit old text and selected rows stay visible.
- Reworked T-Display About, Options, Reading, and Pet Simulator screens to clear between pages and use the larger color display more effectively.
- Fixed T-Display port leftovers including primary-button wiring, Maze Runner constructor setup, Pet Simulator coordinates, and Mouse Emulator redraw state.
- Kept the mature C3 firmware behavior intact while documenting remaining T-Display porting caveats.

## v2.0 b45

- Major architectural overhaul to support multiple boards.
- Introduced FemtoDeck T-Display version for ESP32 boards with 1.14" color LCDs.
- Refactored core application logic into a shared codebase to ensure consistency between C3 and T-Display variants.
- Ports all 25+ apps and games to the new T-Display hardware, making use of full color and higher resolution.
- Integrated WiFi Setup and Communicator (ESP-NOW) apps from the wifi-settings branch.
- Added dual-button navigation for T-Display hardware.
- Updated GitHub Actions to build and package firmware for both platforms automatically.

## v1.1 b44

- Rewrote the README around FemtoDeck C3, removed the inherited game image, and documented current build/upload flow.
- Renamed the on-device `Credits` menu item to `About` with separate License and Credits sections.
- Added concise on-device WTFPL + No Warranty license details.
- Fixed credits pages for Reactor, Reading, and Random Number so they show their developer instead of the repo URL.
- Kept long-press from About detail pages as a return-to-menu shortcut.

## v1.1 b43

- Corrected Femto Field attempts to use one shared five-miss bank across the whole round, rather than resetting attempts for every event.
- After the shared bank is exhausted, the current failed event scores zero and each remaining event in that round gets one pressure try.
- Reset the attempt bank only when a clean round advances to the next harder round.

## v1.1 b42

- Changed Simon hold input to use a shorter local hold threshold; releasing before that counts as tap.
- Cleaned up per-save delete confirmation layout in Save Manager so the selected save and erase/cancel controls do not overlap.

## v1.1 b41

- Changed Femto Field progression from debug mode to five attempts per event until a zero-score event, then one attempt for the rest of that round.
- Made Femto Field end after a round with any zero-score event, otherwise advance to the next harder round while keeping cumulative score.
- Added a hammer throw field animation, including backward throws when the direction is wrong.
- Changed Javelin angle selection to hold-and-release timing instead of tapping an automatically sweeping angle.
- Sorted the Games menu alphabetically while leaving Utilities in their existing order.

## v1.1 b40

- Consumed the boot splash skip tap so it no longer advances the main menu from Games to Utilities.
- Shortened Coin Flipper instructions so they fit on screen.
- Simplified Simon playback and input screens to text-only prompts instead of flashing boxes.
- Made Save Manager confirmations explicitly say `DELETE` before erasing a selected save.

## v1.1 b39

- Changed Femto Field debug flow so qualifying an event advances immediately instead of forcing all five attempts.
- Hid the Hurdles `TAP JUMP` hint after the first jump in an attempt.
- Added fallen hurdle rendering after a failed jump and clamped hurdle drawing to avoid off-screen line flashes.

## v1.1 b38

- Moved Coin Flipper result text away from the coin, with the coin on the right and larger result text on the left.
- Reworked Metronome with instruction pages, a cleaner BPM-only live screen, and a much longer hold-to-exit timeout.
- Moved Pet Simulator into Games, shortened its controls text, and added an idle pet-only wander screen after inactivity.
- Rebuilt Knife Throw around a spinning board with a person, moving reticle, delayed knife travel, and person-hit failure.
- Reworked Reactor display to show current kW and total kWh separately, added stronger rod cooling, cold stall failure, and a radioactive splash symbol.
- Made Simon start with an easy tap cue and show clearer `TAP` / `HOLD` prompts and failure feedback.
- Added an Options Save Manager with per-save deletion and two-step delete-all confirmation.
- Reordered Utilities so Mouse Emulator is second-to-last and Reading is last.

## v1.1 b37

- Made Alien Raiders shield layers visibly disappear as shielded enemies take damage.
- Reduced diagonal spread fire, added stronger late-game enemies, tougher bosses, and boss return fire that the player must dodge.
- Made bomb pickups look like cartoon bombs with a visible fuse and clarified the Alien Raiders intro with `Defend Station` text.
- Reworked Femto Field hurdles into a Jump Run style tap-to-jump event and removed the separate Jump Run source/menu entry.
- Enabled Femto Field debug flow with five attempts per event before advancing, so all events can be tested without qualifying first.

## v1.1 b36

- Added utility apps for dice rolling, coin flipping, random number generation, metronome timing, and a persistent pet simulator.
- Added simple games `Knife Throw`, `Reactor`, and `Simon`, each with start/high-score/prompt screens where applicable.
- Wired the new apps into the Games and Utilities menus and added credits entries.

## v1.1 b35

- Made the FemtoDeck C3 boot splash skippable by pressing the button.
- Retuned City Racer to keep faster road speed while adding enough traffic rows to feel active, with cars-passed scoring and clearer level progression.
- Extended Alien Raiders late-game scaling with stronger shielded enemies, stacking player shields, limited smart-bomb damage, higher weapon tiers, and slow boss waves every 100 kills.
- Fixed Femto Field hurdles so hurdles move toward the runner, with clearer hold/release prompts.

## v1.1 b34

- Retuned City Racer toward faster racing with far fewer traffic rows and much larger gaps between cars.
- Changed Alien Raiders weapon pickups into permanent additive weapon levels so collecting an upgrade cannot downgrade a good weapon.
- Moved Cave Chopper's splash cave line away from the title text.
- Made Femto Field hurdles show the hurdle approaching the runner and changed the airborne prompt away from the confusing `JUMP` label.

## v1.1 b33

- Rebalanced City Racer with slower, sparser early traffic and guaranteed one-direction-safe lane progression.
- Clarified Alien Raiders by moving the base launch animation into a one-time game intro, flipping the player ship forward, reshaping enemy ships, reducing base fire rate, and making powerups rarer/more distinct.
- Made Cave Chopper more forgiving with a smaller chopper collision profile, larger cave gaps, slower initial speed, and gentler vertical acceleration.
- Relaxed Femto Field's first-round qualifying targets and timing cadence so the 100m dash and later events can be tested without requiring near-perfect timing.

## v1.1 b32

- Added `Femto Field`, a one-button multi-event track-and-field game with 100m dash, hurdles, long jump, hammer throw, javelin, and high jump.
- Added cumulative scoring, lives, harder looping rounds, a hard-to-earn extra life threshold, total high score, and per-event point records.
- Added timing cue penalties where early presses count double, plus GPIO8 cue lighting during ideal `PUSH` windows.

## v1.1 b31

- Added an Alien Raiders establishing splash where the player ship launches from the defended station before the view pans toward incoming raiders.
- Changed Breakout '76 paddle control to match the old single-button feel more closely: tap reverses direction and the paddle clamps at screen edges instead of auto-reversing.

## v1.1 b30

- Rebuilt the original five-game set from new source files and removed the old game implementations.
- Replaced Breakout with `Breakout '76`, adding varied launches, paddle-influenced bounces, level patterns, and powerups.
- Replaced Micro Racer with `City Racer`, using forward-facing traffic and fairer level progression.
- Replaced Defender Mini with `Alien Raiders`, adding shielded enemies, progressive difficulty, and temporary upgrades.
- Replaced Heli Cave with `Cave Chopper`, adding a chopper sprite with animated rotors and readable scoring.
- Recreated `Jump Run` from scratch so the original game source set is fully removed.

## v1.1 b29

- Reorganized firmware source into Arduino-compatible `src/games` and `src/apps` folders.
- Kept shared runtime files in the sketch root and updated include paths for the new layout.

## v1.1 b28

- Fixed Reading text wrapping so full lines no longer cause unsigned space calculations to swallow following words.
- Changed the Reading footer to `Tap more` until the final page, then `Tap END`.
- Removed `BSP` suffixes from reading titles and added an `About BSP` attribution/public-domain reading.

## v1.1 b27

- Updated Reading pages to use three body lines plus a footer showing page progress or `Tap END`.
- Moved Reading body text away from the bottom edge to avoid clipping on the physical OLED.

## v1.1 b26

- Replaced the Reading utility placeholders with the selected BSP reading set.
- Synced the browser simulator Reading utility with the firmware readings.

## v1.1 b25

- Added a `Reading` utility app with selectable reading slots and automatic text wrapping/paging.
- Added neutral placeholder reading text so final long-form text can be pasted locally later.

## v1.1 b24

- Lowered Blackjack's `2 deck shoe` splash text so it no longer overlaps the card/chip art.
- Added a `Femto OS` credits page for `thedarkfalcon`.
- Changed the boot splash to `FemtoDeck` with `C3` as the model line and the build string on-screen.
- Shortened the root menu title to `FemtoDeck` and renamed the sketch folder/file to `femtodeck-c3`.
- Renamed the GitHub Actions workflow to `build-femtodeck-c3.yml` and removed the old sponsor funding file.

## v1.1 b23

- Renamed the shared launchable base from `Game` to `App` and renamed utility/system files and classes to `*App`.
- Replaced the flat firmware menu with a root menu containing `Games`, `Utilities`, `Options`, and `Credits`.
- Added a short `C3 FemtoDeck` boot splash screen.
- Updated the browser simulator to mirror the boot splash and folder-style menu.

## v1.1 b22

- Moved `Mouse Emulator` vertical wiggle tuning values into top-level constants and increased the bounded vertical wander limit to 20 pixels.

## v1.1 b21

- Added bounded vertical jitter to `Mouse Emulator` movement so the cursor wiggles more like the original S3 mouse script while keeping the C3 update loop non-blocking.

## v1.1 b20

- Renamed `Mouse Jiggler` to `Mouse Emulator` in the device menu, splash screen, credits, and browser simulator.

## v1.1 b19

- Reworked `Mouse Jiggler` to use a small local NimBLE HID mouse implementation instead of the older `ESP32 BLE Mouse` wrapper, reducing BLE task overhead on the ESP32-C3.
- Reset Mouse Jiggler movement state whenever the game launches so stale movement phases cannot carry between runs.

## v1.1 b18

- Added `Counter` utility: a simple counting app (tap increments, hold resets). Currently the only change in this build; more updates planned for future builds.

## v1.1 b17

- Added and refined two timer utilities: `Stopwatch` and `Countdown` (selection UI, top/bottom hints, hold/tap behaviors).
- Fixed menu overlay drawing over utility UIs by giving timer apps a custom overlay; start screens now match other utility apps (`Tap to start` prompts).
- Adjusted hint placement and sizing to avoid clipping on the 72x40 OLED.
- Updated the browser simulator (`sim/`) to mirror the above UI and build-text to `v1.1 b17`.
- Updated repository agent instructions to include a compact project overview and contributor guidance.

## v1.1 b16

- Fixed Tower Stacker's near-perfect placement forgiveness so it snaps cleanly onto the supported block instead of drawing unsupported overhang.

## v1.1 b15

- Made Pipe Mania much more forgiving by increasing build time before goo starts, slowing early goo flow, and reducing hold-to-place delay.
- Added persistent Mini Lander best-level tracking and a best-level intro page.
- Added splash/top-score/prompt pages and persistent scores to Micro Racer, Heli Cave, Breakout, Defender Mini, and Jump Run.
- Added Micro Racer levels that increase speed every 50 cars passed.
- Added Heli Cave survival-time scoring and a stronger gradual speed ramp.
- Added Breakout level progression and cumulative brick-broken scoring.
- Added Defender Mini health so enemies passing in any lane damage the player.
- Reworked Tower Stacker into 10-block tower stages with wider resets, faster later stages, and gentler width loss.
- Updated Credits to show atomic14 as creator and thedarkfalcon as modifier for the original games, plus `git.new/esp32games`.

## v1.1 b14

- Fixed intro page rotation so every game starts on its splash screen when selected, then advances to best score, then `Press to Start`.
- Removed the old uptime-based intro page helper to prevent future screens from starting mid-cycle.

## v1.1 b13

- Changed game intro rotation to three standalone pages: splash art/title, top score/best info, then `Press to Start`.
- Removed start prompts from splash and high-score pages so they no longer clutter the art or score layout.
- Added persistent Need Speed best-run tracking with best level cleared, run time, and player initials.
- Updated the browser simulator to match the new intro flow and Need Speed best-run behavior.

## v1.1 b12

- Fixed Options initials save and Credits long-hold behavior so both return directly to the main menu.
- Added clearer `Tap start`/`Tap retry` prompts to splash, score, and utility screens.
- Made Need Speed level 1 more reachable, added explicit `Lost` result text, and shows the target speed/time when you miss.
- Added Fishing Flick's hook-the-fish timing phase and softened tension recovery so fight spikes are less punishing.
- Moved Blackjack hand totals left to avoid the OLED border and added clearer start prompts.
- Widened Tiny Golf's tight early wall gap.
- Added a desktop level editor at `sim/editor.html` for Tiny Golf and maze wall layouts.
- Synced the browser simulator with the gameplay/prompt changes.

## v1.1 b11

- Added `build.sh`, a Bash build helper for Linux and macOS that increments `Version.h` before running `arduino-cli compile`.
- Bumped the firmware build for device upload.

## v1.1 b10

- Added `Version.h` with `BuildInfo::VERSION_TEXT` and `BuildInfo::BUILD_TEXT`.
- Displayed the current build number on the Credits start and end screens.
- Added `build.ps1` to increment the build number before running `arduino-cli compile`.
- Added this release notes file.

## v1.1 b9

- Mini Lander now shows the safe landing velocity on the crash/end screen as a speed-limit style sign.
- Mini Lander safe landing velocity now varies by level.
- Mini Lander level briefing now ignores input for 1 second, then waits for a fresh tap before starting.
- Need Speed now has 5 levels, increasing target speed and rev difficulty between levels.
- Need Speed now shows a level-clear interstitial and total run time.
- Fishing Flick now has level progression with larger, harder fish.
- Fishing Flick tension now spikes during fish fights, can break at high tension, and can lose the fish at zero slack.

## v1.1 b8

- Added an `Options` menu entry for two-character player initials.
- Added shared `PlayerProfile` storage for initials.
- High-score games now save initials alongside new best scores.
- Intro screens for score-saving games now alternate between splash art and top-score display.
- Added `Maze Collector`, a key-collection variant of Maze Runner with a locked exit.
- Improved Maze Runner defensiveness around no-option choice states.
- Reworked Pipe Mania to make the selected square clearer, slow early levels, and invert filled liquid cells.
- Reworked Blackjack with bet selection, animated dealing, a persistent two-deck shoe, reshuffle behavior, and delayed dealer actions.
- Updated Credits with per-game pages for new games and helpers.

## v1.1 b7

- Added browser emulator under `sim/`.
- Added a crisp bitmap font to the emulator so the scaled OLED text remains readable.
- Emulator supports the one-button menu, GPIO8 LED indicator, local score storage, and playable versions of the new games.

## v1.1 b6

- Added Noon Shooter.
- Added Fishing Flick with GPIO8 tension warning.
- Added Maze Runner with generated mazes and one-button intersection choices.
- Added Pipe Mania adapted for one-button pipe placement.
- Added Blackjack.

## v1.1 b5

- Added Need Speed, a one-button drag-racing dashboard game.
- Added traffic-light launch countdown.
- Added round tachometer, shift quality messages, and GPIO8 shift-light behavior.

## v1.1 b4

- Added Credits as a menu entry.
- Added per-game credit pages for the original atomic14 games and new local games.
- Added GPIO8 support patterns for games that use the onboard LED.

## v1.1 b3

- Added Mini Lander.
- Added moon splash screen, level briefings, fuel/altitude/velocity HUD, landing pad, thrust physics, crash/landing outcomes, and GPIO8 burn/low-fuel cues.

## v1.1 b2

- Added Tiny Golf.
- Added rotating aim, power selection, walls, cups, bounce guides, multi-hole course flow, and best-score persistence.
- Tuned early golf courses, bumper placement, cup forgiveness, aim speed near the cup, and shot behavior.

## v1.1 b1

- Added Tower Stacker.
- Added score display and persistent high score.
- Added end-screen score display.
- Added shared end-screen input lockout to reduce accidental restarts across games.

## v1.0 b0

- Original atomic14 project baseline.
- Included Breakout, Micro Racer, Defender Mini, Jump Run, and Heli Cave.
- Included the original one-button menu flow and ESP32-C3 OLED display setup.
