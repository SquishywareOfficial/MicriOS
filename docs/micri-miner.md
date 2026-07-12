# Micri Miner

Micri Miner is MicriOS's solo mining utility. It connects directly to a Stratum pool using saved MicriOS WiFi profiles and a small mining engine adapted from NerdMiner_v2.

It is mostly a tiny-device experiment. Expected mining odds are extremely small.

## Setup Flow

1. Open **WiFi Setup** and save at least one WiFi network.
2. Open **Micri Miner**.
3. Launch the miner setup portal from the app.
4. Connect your phone or computer to the setup AP.
5. Save wallet and pool settings.
6. Return to Micri Miner and start mining.

Micri Miner settings are separate from WiFi settings. WiFi Setup controls network profiles; Micri Miner Setup controls wallet and pool details.

## Default Settings

- Pool: `public-pool.io`
- Port: `21496`
- Password: `x`
- Default wallet: `bc1qfreyhgyjj03pk60jdpym2tmcx780jmsgcvj8gl`
- Worker name: `Micri<MACADDRESS>`, for example `Micri001A2B3C4D5E`
- Stratum username: `<wallet>.<worker>`

Change the wallet in the setup portal if you want any accepted shares to be credited to your own address.

## Setup Portal

- SSID: `MicriMiner Setup`
- Password: `micriminer`

The portal saves:

- wallet address
- pool host
- pool port
- pool password

Saved settings live in the `miner` Preferences namespace. **Options / Save Manager / Miner** clears these settings.

## T-Display Pages

The T-Display miner has multiple status pages:

- hashrate dashboard
- submitted/accepted/rejected shares
- session totals
- lifetime totals
- pool state
- BTC price lookup
- setup
- reset

Button behavior:

- Button 1: start/stop or page action.
- Button 2: next page.
- Button 2 hold: exit to menu and stop mining cleanly.

## ESP32-C3 OLED UI

The C3 OLED version has a compact status UI to fit the `72x40` screen. It uses the same miner settings and shared mining logic, but the screen has much less room for details.

## Notes

- T-Display/classic ESP32 uses a faster hardware SHA path.
- ESP32-C3 is much slower and is generally more useful as a low-power experiment or Distributed Miner slave.
- Mining tasks stop when leaving the app.
- Optional serial miner logs are available in the T-Display build for profiling.

## License

Micri Miner includes MIT-attributed mining and Stratum logic adapted from NerdMiner_v2. See [../licenses/NerdMiner_v2-MIT.txt](../licenses/NerdMiner_v2-MIT.txt).
