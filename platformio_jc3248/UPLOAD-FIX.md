# Upload — ESP32-S3 (COM31)

## Error: `OSError(22) device which does not exist`

Nangyayari ito kapag sine-reset ng esptool ang board — **nawawala ang COM31** sa Windows.

**Fix na naka-set sa `platformio.ini`:** `--before=no_reset` (walang auto-reset).

---

## Paano mag-upload (ESP32-S3 USB)

### Hakbang 1 — Isara ang Serial Monitor
PlatformIO → **Stop** monitor. Isara VS Code kung may PIO doon.

### Hakbang 2 — Download mode
1. **Hold** ang **BOOT** button (huwag bitawan pa)
2. (Optional) Pindot **RESET** nang isang beses
3. Sa PlatformIO click **Upload** (`jc3248w535c`)
4. **Bitawan BOOT** kapag nakita mo na **"Writing at 0x..."**

### Hakbang 3 — Serial Monitor (pagkatapos)
```bash
pio device monitor -b 115200
```

---

## Kung "Connecting..." hanggang fail

- Subukan **ibang USB cable** (data, hindi charge-only)
- Direct sa PC USB port (hindi hub)
- Unplug 10 sec → plug → ulitin Hakbang 2
- Device Manager → tingnan kung may **COM31** pa

## Kung "COM PORT LOCKED"

May program na nakahawak — isara lahat ng monitor at VS Code windows.
