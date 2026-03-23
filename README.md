# CarDashboard — ESP32 CYD Fuel Gauge

A real-time fuel level gauge for the ESP32 CYD (Cheap Yellow Display), reading OBD2 data from an ELM327 Bluetooth Classic adapter.

## Hardware

| Component | Detail |
|-----------|--------|
| Display board | ESP32-2432S028R "CYD" — ESP32-WROOM-32 with ST7789 320x240 TFT |
| OBD2 adapter | Any ELM327 Bluetooth Classic dongle |
| Power | 5V buck converter from a fuse tap to the CYD's USB port (min 500 mA) |

## What It Does

- Connects to an ELM327 adapter over Bluetooth Classic
- Queries OBD2 PID `0x2F` (fuel tank level, 0-100%)
- Displays a horizontal bar gauge with the fuel percentage
- Bar colour changes: green (>50%), yellow (25-50%), red (<25%)
- Shows status screens: "CONNECTING...", "INITIALISING OBD...", "NO SIGNAL"
- Automatically reconnects if the adapter drops out

## Display

All rendering uses a full-screen `TFT_eSprite` pushed in a single SPI transaction — no flicker. The percentage is drawn in a large 7-segment font (Font 7) with the `%` symbol in Font 4 beside it.

If the ESP32 runs low on RAM (Bluetooth + sprite compete for ~320 KB), the display module automatically falls back to 8-bit colour depth.

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
pio run                  # Compile
pio run -t upload        # Flash via USB
pio device monitor       # Serial monitor (115200 baud)
```

TFT_eSPI pin configuration is defined entirely in `platformio.ini` build flags — there is no separate `User_Setup.h` file.

## Configuration

All tuneable values live in `src/config.h`:

| Constant | Default | Purpose |
|----------|---------|---------|
| `CFG_BT_DEVICE_NAME` | `"OBDII"` | Bluetooth name of your ELM327 adapter |
| `CFG_BT_PIN` | `"1234"` | Bluetooth PIN code |
| `CFG_POLL_INTERVAL_MS` | `3000` | How often to query fuel level (ms) |
| `CFG_ELM_TIMEOUT_MS` | `5000` | ELM327 init timeout — increase for cheap clones |
| `CFG_CONSECUTIVE_ERROR_THRESHOLD` | `3` | Failed polls before showing "NO SIGNAL" |
| `CFG_THRESH_GREEN` / `CFG_THRESH_YELLOW` | `50` / `25` | Colour change thresholds (%) |

## Project Structure

```
src/
  main.cpp            State machine loop, module wiring
  config.h            All tuneable constants
  state_machine.h/cpp AppState enum and transitions
  gauge_display.h/cpp Sprite-based TFT rendering
  obd_reader.h/cpp    BluetoothSerial + ELMduino wrapper
```

## State Machine

```
BT_CONNECTING ──► OBD_INIT ──► RUNNING
      ▲                           │
      └──── ERROR ◄───────────────┘
             (5s delay)    (3+ consecutive poll failures
                            or BT disconnect)
```

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Display blank or garbled | Your CYD may have an ILI9341 instead of ST7789. Change `-DST7789_DRIVER=1` to `-DILI9341_DRIVER=1` in `platformio.ini` and remove `-DTFT_INVERSION_OFF=1` |
| Colours swapped (red/blue) | Check `-DTFT_RGB_ORDER=TFT_BGR` is present in build flags |
| ELM327 won't connect | Try a different `CFG_BT_DEVICE_NAME` — some clones advertise as "OBD2", "ELM327", etc. |
| "NO SIGNAL" immediately | Increase `CFG_ELM_TIMEOUT_MS` to `30000` for cheap ELM327 clones that do a full protocol search |
| Crash on boot (OOM) | The 16-bit sprite + Bluetooth uses ~290 KB. The display auto-falls back to 8-bit. If still crashing, check Serial output |
| Vehicle shows 0% or erratic fuel | Some vehicles don't support PID 0x2F, or return non-standard values |
